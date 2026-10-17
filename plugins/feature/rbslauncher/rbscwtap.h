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

#ifndef INCLUDE_FEATURE_RBSCWTAP_H_
#define INCLUDE_FEATURE_RBSCWTAP_H_

#include <vector>

#include <QObject>
#include <QPointer>

#include "rbscwdecoder.h"

class ChannelAPI;
class DataFifo;
class MessageQueue;
class ObjectPipe;

// Takes the demodulated audio of a channel (its "demod" data pipe, as the SDRangel Morse decoder
// feature does) and runs the RBS Morse decoder on it, in the GUI thread
class RBSCWTap : public QObject
{
    Q_OBJECT
public:
    explicit RBSCWTap(QObject *parent = nullptr);
    ~RBSCWTap() override;

    void attach(ChannelAPI *channel);  //!< Null: detach
    ChannelAPI *channel() const { return m_channel; }
    RBSCWDecoder& decoder() { return m_decoder; }
    int sampleRate() const { return m_sampleRate; }

signals:
    void decoded();  //!< Characters or scope points to take from the decoder

private:
    ChannelAPI *m_channel;
    ObjectPipe *m_dataPipe;
    QPointer<DataFifo> m_fifo;
    QPointer<MessageQueue> m_reportQueue;
    int m_sampleRate;
    RBSCWDecoder m_decoder;
    std::vector<float> m_samples;

    void detach();
    void handleReports();
    void readFifo();
};

#endif // INCLUDE_FEATURE_RBSCWTAP_H_
