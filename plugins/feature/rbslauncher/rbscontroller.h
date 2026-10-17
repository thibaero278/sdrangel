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
#include <QVariant>

#include "rbsmode.h"

class WebAPIAdapterInterface;
class DeviceSet;
class ChannelAPI;

// Sets SDRangel up for a mode: an Rx device set with the chosen device (the RSP1B by default),
// the device settings of the mode, its decoder channel, and the device running. Also manages
// the listening channel (NFM, AM, USB...) that the Radio section controls.
//
// Device set and channel creation are asynchronous in the GUI (they go through the main
// window's message queue), so the work is a queue of steps, each with a condition that is
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

    struct Listen
    {
        QString m_demod;        //!< "", "NFM", "AM", "USB", "LSB", "CW" or "WFM"
        int m_bandwidth = 0;    //!< Hz
        int m_squelch = -40;    //!< dB
        int m_volume = 50;      //!< %, 50 = the demodulator's default level
    };

    explicit RBSController(WebAPIAdapterInterface *webAPIAdapterInterface, QObject *parent = nullptr);

    void setDevice(const QString& hwType, int sequence);  //!< Used from the next startMode
    const QString& getHwType() const { return m_hwType; }
    bool isSDRplay() const { return m_hwType == m_sdrplayHwType; }
    bool isAudioSource() const { return m_hwType == "AudioInput"; }  //!< Sound card: audio, no frequency
    bool hasDevice() const;  //!< The device set has the chosen device

    void startMode(const RBSMode& mode);
    void setListen(const Listen& listen);  //!< Replaces the listening channel ("" demod: none)
    void updateListen(const Listen& listen);  //!< Bandwidth, squelch and volume of the current one
    void startDevice();
    void stopDevice();
    bool isDeviceRunning() const;

    State getState() const { return m_state; }
    const RBSMode& getMode() const { return m_mode; }
    DeviceSet *getDeviceSet() const;   //!< Null if it has gone
    int getDeviceSetIndex() const;     //!< -1 if none
    ChannelAPI *getChannel() const;    //!< The mode's decoder channel, null if none
    int getChannelIndex() const;
    int getListenChannelIndex() const;

    qint64 getCenterFrequency() const;
    int getSampleRate() const;         //!< Baseband rate, after decimation
    //! Frequency of the decoder: device centre + channel offset (+ the CW pitch, see RBSMode::m_tuneShift)
    qint64 getTunedFrequency() const { return getCenterFrequency() + m_channelOffset + m_mode.m_tuneShift; }
    void setTunedFrequency(qint64 frequency);  //!< Moves the device, keeping the channel offset
    int getChannelOffset() const { return m_channelOffset; }
    void setChannelOffset(int offset);  //!< Moves the decoder and listening channels, not the device
    bool setDeviceSetting(const QString& key, int value);
    bool getDeviceSetting(const QString& key, int& value) const;
    bool setFeatureSetting(const QString& key, double value);  //!< Of the mode's decoder feature (Morse...)

    static const char* const m_sdrplayHwType;
    static const char* const m_channelTitle;      //!< Title of the decoder channel RBS adds
    static const char* const m_listenTitle;       //!< Title of the listening channel RBS adds
    static const char* const m_featureTitle;      //!< Title of the decoder feature RBS adds (Morse...)

    static QString listenChannelType(const QString& demod);
    static int defaultBandwidth(const QString& demod);
    static bool hasSquelch(const QString& demod);

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
    QString m_hwType;
    int m_sequence;
    RBSMode m_mode;
    State m_state;
    QList<Step> m_steps;
    bool m_stepStarted;
    bool m_modeStartPending;
    QElapsedTimer m_stepTimer;
    QTimer m_pollTimer;

    const void *m_deviceSet;           //!< Identity of our device set (compared, never dereferenced as such)
    uint64_t m_channelUID;             //!< Our decoder channel, 0 if none
    uint64_t m_listenUID;              //!< Our listening channel, 0 if none
    uint64_t m_featureUID;             //!< Our decoder feature, 0 if none
    int m_channelOffset;               //!< Current offset of the decoder and listening channels
    QSet<uint64_t> m_channelsBefore;
    QSet<uint64_t> m_featuresBefore;
    bool m_featureSelectRetried;
    int m_deviceSetCountBefore;

    void setState(State state, const QString& message);
    void addStep(const QString& label, std::function<bool(QString&)> action, std::function<bool()> done = nullptr,
        int timeoutMs = 5000, const QString& timeoutError = QString());
    void runSteps();
    void addChannelSteps(const QString& label, const QString& channelType, uint64_t *uid);
    DeviceSet *findRxDeviceSet() const;
    bool deviceAvailable() const;
    QString deviceName() const;
    QSet<uint64_t> channelUIDs() const;
    int channelIndex(uint64_t uid) const;
    ChannelAPI *channel(uint64_t uid) const;
    QList<int> rbsChannelIndexes() const;
    QSet<uint64_t> featureUIDs() const;
    int featureIndex(uint64_t uid) const;
    QList<int> rbsFeatureIndexes() const;
    bool featureSelectedChannel() const;
    bool selectFeatureChannel();
    void addFeatureSteps();
    void applyListenSettings(const Listen& listen, bool all);

private slots:
    void poll();
};

#endif // INCLUDE_FEATURE_RBSCONTROLLER_H_
