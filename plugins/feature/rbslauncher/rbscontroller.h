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

#ifndef INCLUDE_FEATURE_RBSCONTROLLER_H_
#define INCLUDE_FEATURE_RBSCONTROLLER_H_

#include <functional>

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTimer>

#include "rbsmode.h"

class WebAPIAdapterInterface;
class DeviceSet;
class ChannelAPI;

// Sets SDRangel up for a mode: an Rx device set with the RSP1B, the device settings of the
// mode, its decoder channel, and the device running.
//
// Device set and channel creation are asynchronous in the GUI (they go through the main
// window's message queue), so the work is a list of steps, each with a condition that is
// polled until it holds or the step times out. Everything runs in the main thread.
class RBSController : public QObject
{
    Q_OBJECT
public:
    enum State {
        Idle,
        Starting,
        Running,
        Error
    };

    explicit RBSController(WebAPIAdapterInterface *webAPIAdapterInterface, QObject *parent = nullptr);

    void startMode(const RBSMode& mode);
    void startDevice();
    void stopDevice();
    bool isDeviceRunning() const;

    State getState() const { return m_state; }
    const RBSMode& getMode() const { return m_mode; }
    DeviceSet *getDeviceSet() const;   //!< Null if it has gone
    int getDeviceSetIndex() const;     //!< -1 if none
    ChannelAPI *getChannel() const;    //!< The mode's decoder channel, null if none
    int getChannelIndex() const;

    qint64 getCenterFrequency() const;
    int getSampleRate() const;         //!< Baseband rate, after decimation
    qint64 getTunedFrequency() const { return getCenterFrequency() + m_mode.m_channelOffset; }
    void setTunedFrequency(qint64 frequency);  //!< Moves the device, keeping the channel offset
    bool setDeviceSetting(const QString& key, int value);
    bool getDeviceSetting(const QString& key, int& value) const;

    static const char* const m_hwType;
    static const char* const m_channelTitle;  //!< Title given to the channels RBS adds

signals:
    void stateChanged(RBSController::State state, const QString& message);
    void modeStarted(const RBSMode& mode);

private:
    struct Step
    {
        QString m_label;
        std::function<bool(QString& error)> m_action;  //!< Returns false on error
        std::function<bool()> m_done;                  //!< Null: done once the action ran
        int m_timeoutMs;
        QString m_timeoutError;
    };

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    RBSMode m_mode;
    State m_state;
    QList<Step> m_steps;
    bool m_stepStarted;
    QElapsedTimer m_stepTimer;
    QTimer m_pollTimer;

    const void *m_deviceSet;           //!< Identity of our device set (compared, never dereferenced as such)
    uint64_t m_channelUID;             //!< Our decoder channel, 0 if none
    QSet<uint64_t> m_channelsBefore;
    int m_deviceSetCountBefore;

    void setState(State state, const QString& message);
    void addStep(const QString& label, std::function<bool(QString&)> action, std::function<bool()> done = nullptr,
        int timeoutMs = 5000, const QString& timeoutError = QString());
    DeviceSet *findRxDeviceSet() const;
    static bool rspAvailable();
    QSet<uint64_t> channelUIDs() const;
    QList<int> rbsChannelIndexes() const;

private slots:
    void poll();
};

#endif // INCLUDE_FEATURE_RBSCONTROLLER_H_
