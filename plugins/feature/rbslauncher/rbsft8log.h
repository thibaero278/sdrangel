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

#ifndef INCLUDE_FEATURE_RBSFT8LOG_H_
#define INCLUDE_FEATURE_RBSFT8LOG_H_

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

// The FT8 demodulator only gives its decodes to its own GUI, but it can write them to a daily
// log file. This follows that file, as tail -f would, and emits each new decode.
class RBSFT8Log : public QObject
{
    Q_OBJECT
public:
    struct Decode
    {
        QDateTime m_dateTime;  //!< UTC, start of the 15 s period
        double m_baseFrequencyMHz;
        QString m_mode;        //!< FT8 or FT4
        int m_snr;
        double m_dt;
        int m_df;
        QString m_call1;       //!< CQ, or the called station
        QString m_call2;       //!< Calling station
        QString m_loc;         //!< Locator, report or RR73
    };

    explicit RBSFT8Log(QObject *parent = nullptr);

    //! Follow the log of the FT8 channel at that position. Only lines written from now on are reported.
    void start(int deviceSetIndex, int channelIndex);
    void stop();

    static QString logsPath();
    static bool parseLine(const QString& line, Decode& decode);

signals:
    void decoded(const RBSFT8Log::Decode& decode);

private:
    QTimer m_timer;
    QString m_channelReference;
    QString m_filePath;
    qint64 m_position;

    QString currentFilePath() const;

private slots:
    void poll();
};

#endif // INCLUDE_FEATURE_RBSFT8LOG_H_
