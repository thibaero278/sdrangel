///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Thibaud (RBS)                                              //
// Some code by Claude                                                           //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "channel/channelapi.h"
#include "dsp/datafifo.h"
#include "maincore.h"
#include "pipes/objectpipe.h"
#include "util/messagequeue.h"

#include "rbscwtap.h"

RBSCWTap::RBSCWTap(QObject *parent) :
    QObject(parent),
    m_channel(nullptr),
    m_dataPipe(nullptr),
    m_sampleRate(0)
{
}

RBSCWTap::~RBSCWTap()
{
    detach();
}

void RBSCWTap::attach(ChannelAPI *channel)
{
    if (channel == m_channel) {
        return;
    }

    detach();

    if (!channel) {
        return;
    }

    MainCore *mainCore = MainCore::instance();
    m_channel = channel;
    m_dataPipe = mainCore->getDataPipes().registerProducerToConsumer(channel, this, "demod");

    if (m_dataPipe)
    {
        // The channel going: forget it (the pipe and its FIFO are freed by the pipes GC)
        connect(m_dataPipe, &ObjectPipe::toBeDeleted, this, [this](int reason, QObject *object) {
            if ((reason == 0) && (object == m_channel))
            {
                m_channel = nullptr;
                m_dataPipe = nullptr;
                m_fifo = nullptr;
                m_reportQueue = nullptr;
            }
        });
        m_fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (m_fifo)
        {
            m_fifo->setSize(96000);
            connect(m_fifo.data(), &DataFifo::dataReady, this, &RBSCWTap::readFifo, Qt::QueuedConnection);
        }
    }

    // The audio rate comes as a report, asked for now and sent again when it changes
    ObjectPipe *messagePipe = mainCore->getMessagePipes().registerProducerToConsumer(channel, this, "reportdemod");

    if (messagePipe)
    {
        m_reportQueue = qobject_cast<MessageQueue*>(messagePipe->m_element);

        if (m_reportQueue) {
            connect(m_reportQueue.data(), &MessageQueue::messageEnqueued, this, &RBSCWTap::handleReports, Qt::QueuedConnection);
        }
    }

    channel->getInputMessageQueue()->push(MainCore::MsgChannelDemodQuery::create());
    m_decoder.reset();
}

void RBSCWTap::detach()
{
    if (!m_channel) {
        return;
    }

    MainCore *mainCore = MainCore::instance();

    if (m_fifo) {
        disconnect(m_fifo.data(), nullptr, this, nullptr);
    }
    if (m_reportQueue) {
        disconnect(m_reportQueue.data(), nullptr, this, nullptr);
    }
    if (m_dataPipe) {
        disconnect(m_dataPipe, nullptr, this, nullptr);
    }

    mainCore->getDataPipes().unregisterProducerToConsumer(m_channel, this, "demod");
    mainCore->getMessagePipes().unregisterProducerToConsumer(m_channel, this, "reportdemod");
    m_channel = nullptr;
    m_dataPipe = nullptr;
    m_fifo = nullptr;
    m_reportQueue = nullptr;
}

void RBSCWTap::handleReports()
{
    if (!m_reportQueue) {
        return;
    }

    Message *message;

    while ((message = m_reportQueue->pop()) != nullptr)
    {
        if (MainCore::MsgChannelDemodReport::match(*message))
        {
            const MainCore::MsgChannelDemodReport& report = (const MainCore::MsgChannelDemodReport&) *message;

            if ((report.getChannelAPI() == m_channel) && (report.getSampleRate() > 0) && (report.getSampleRate() != m_sampleRate))
            {
                m_sampleRate = report.getSampleRate();
                m_decoder.setSampleRate(m_sampleRate);
                qDebug("RBSCWTap::handleReports: audio at %d S/s", m_sampleRate);

                if (m_fifo) {
                    m_fifo->setSize(2 * m_sampleRate);
                }
            }
        }

        delete message;
    }
}

void RBSCWTap::readFifo()
{
    if (!m_fifo) {
        return;
    }

    bool any = false;

    while (m_fifo && (m_fifo->fill() > 0))
    {
        QByteArray::iterator part1begin, part1end, part2begin, part2end;
        DataFifo::DataType dataType;
        unsigned int count = m_fifo->readBegin(m_fifo->fill(), &part1begin, &part1end, &part2begin, &part2end, dataType);

        // Mono 16 bit audio (I16); binaural (CI16) is I/Q of the audio, its I is the audio too
        int step = dataType == DataFifo::DataTypeCI16 ? 2 : dataType == DataFifo::DataTypeI16 ? 1 : 0;

        if ((step > 0) && (m_sampleRate > 0))
        {
            for (auto part : {std::make_pair(part1begin, part1end), std::make_pair(part2begin, part2end)})
            {
                int n = (int) ((part.second - part.first) / (int) sizeof(qint16));

                if (n <= 0) {
                    continue;
                }

                const qint16 *samples = reinterpret_cast<const qint16*>(&(*part.first));

                m_samples.resize(n / step);

                for (int i = 0; i < n / step; i++) {
                    m_samples[i] = samples[i * step] / 32768.0f;
                }

                m_decoder.process(m_samples.data(), (int) m_samples.size());
                any = true;
            }
        }

        m_fifo->readCommit(count);
    }

    if (any) {
        emit decoded();
    }
}
