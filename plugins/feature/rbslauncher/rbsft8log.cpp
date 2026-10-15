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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimeZone>

#include "rbsft8log.h"

RBSFT8Log::RBSFT8Log(QObject *parent) :
    QObject(parent),
    m_position(0)
{
    connect(&m_timer, &QTimer::timeout, this, &RBSFT8Log::poll);
}

// Same folder as FT8DemodWorker uses
QString RBSFT8Log::logsPath()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return dir.absolutePath() + "/ft8/logs";
}

void RBSFT8Log::start(int deviceSetIndex, int channelIndex)
{
    m_channelReference = QString("d%1c%2").arg(deviceSetIndex).arg(channelIndex);
    m_filePath = currentFilePath();
    QFileInfo fi(m_filePath);
    m_position = fi.exists() ? fi.size() : 0;
    m_timer.start(1000);
}

void RBSFT8Log::stop()
{
    m_timer.stop();
    m_channelReference.clear();
}

// The worker names the file after the UTC date of the period: yyyyMMdd_dXcY.txt
QString RBSFT8Log::currentFilePath() const
{
    QString fileName = QString("%1_%2.txt").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd")).arg(m_channelReference);
    return QDir(logsPath()).absoluteFilePath(fileName);
}

void RBSFT8Log::poll()
{
    QString filePath = currentFilePath();

    if (filePath != m_filePath)
    {
        // New day, new file
        m_filePath = filePath;
        m_position = 0;
    }

    QFile file(m_filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    if (file.size() < m_position) {
        m_position = 0; // file was replaced
    }

    file.seek(m_position);

    // canReadLine() only looks at what is already buffered, which is nothing right after the
    // seek: read until the end instead, leaving a line still being written for the next poll
    while (!file.atEnd())
    {
        QByteArray bytes = file.readLine();

        if (!bytes.endsWith('\n')) {
            break;
        }

        QString line = QString::fromUtf8(bytes).trimmed();
        m_position = file.pos();
        Decode decode;

        if (parseLine(line, decode)) {
            emit decoded(decode);
        }
    }
}

// Line written by FT8DemodWorker::processBuffer, e.g.
// 261015_191015    14.074 Rx FT8    -12  0.2 1234 CQ F4XYZ JN25
bool RBSFT8Log::parseLine(const QString& line, Decode& decode)
{
    static const QRegularExpression spaces("\\s+");
    QStringList f = line.split(spaces, Qt::SkipEmptyParts);

    if ((f.size() < 9) || (f[2] != "Rx")) {
        return false;
    }

    decode.m_dateTime = QDateTime::fromString("20" + f[0], "yyyyMMdd_HHmmss");
    decode.m_dateTime.setTimeZone(QTimeZone::UTC);
    decode.m_baseFrequencyMHz = f[1].toDouble();
    decode.m_mode = f[3];
    decode.m_snr = f[4].toInt();
    decode.m_dt = f[5].toDouble();
    decode.m_df = f[6].toInt();
    decode.m_call1 = f[7];
    decode.m_call2 = f[8];
    decode.m_loc = f.size() > 9 ? f[9] : QString();

    return decode.m_dateTime.isValid();
}
