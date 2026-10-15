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

#include <QDebug>

#include "SWGChannelSettings.h"
#include "SWGDeviceListItem.h"
#include "SWGErrorResponse.h"
#include "SWGSuccessResponse.h"

#include "channel/channelapi.h"
#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "device/deviceenumerator.h"
#include "device/deviceset.h"
#include "dsp/devicesamplesource.h"
#include "webapi/webapiadapterinterface.h"
#include "maincore.h"

#include "rbscontroller.h"

const char* const RBSController::m_hwType = "SDRplayV3";

RBSController::RBSController(WebAPIAdapterInterface *webAPIAdapterInterface, QObject *parent) :
    QObject(parent),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_state(Idle),
    m_stepStarted(false),
    m_deviceSet(nullptr),
    m_channelUID(0),
    m_deviceSetCountBefore(0)
{
    m_pollTimer.setInterval(100);
    connect(&m_pollTimer, &QTimer::timeout, this, &RBSController::poll);
}

void RBSController::setState(State state, const QString& message)
{
    m_state = state;
    emit stateChanged(state, message);
}

void RBSController::addStep(const QString& label, std::function<bool(QString&)> action, std::function<bool()> done,
    int timeoutMs, const QString& timeoutError)
{
    m_steps.append(Step{label, action, done, timeoutMs, timeoutError});
}

DeviceSet *RBSController::getDeviceSet() const
{
    for (auto deviceSet : MainCore::instance()->getDeviceSets())
    {
        if (deviceSet == m_deviceSet) {
            return deviceSet;
        }
    }

    return nullptr;
}

int RBSController::getDeviceSetIndex() const
{
    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    for (int i = 0; i < (int) deviceSets.size(); i++)
    {
        if (deviceSets[i] == m_deviceSet) {
            return i;
        }
    }

    return -1;
}

ChannelAPI *RBSController::getChannel() const
{
    DeviceSet *deviceSet = getDeviceSet();

    if (!deviceSet || (m_channelUID == 0)) {
        return nullptr;
    }

    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = deviceSet->getChannelAt(i);

        if (channel && (channel->getUID() == m_channelUID)) {
            return channel;
        }
    }

    return nullptr;
}

int RBSController::getChannelIndex() const
{
    DeviceSet *deviceSet = getDeviceSet();

    if (!deviceSet || (m_channelUID == 0)) {
        return -1;
    }

    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = deviceSet->getChannelAt(i);

        if (channel && (channel->getUID() == m_channelUID)) {
            return i;
        }
    }

    return -1;
}

QSet<uint64_t> RBSController::channelUIDs() const
{
    QSet<uint64_t> uids;
    DeviceSet *deviceSet = getDeviceSet();

    if (deviceSet)
    {
        for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
        {
            if (ChannelAPI *channel = deviceSet->getChannelAt(i)) {
                uids.insert(channel->getUID());
            }
        }
    }

    return uids;
}

// The device set already used, else the first Rx one, preferring one that has the RSP1B
DeviceSet *RBSController::findRxDeviceSet() const
{
    if (DeviceSet *deviceSet = getDeviceSet()) {
        return deviceSet;
    }

    DeviceSet *firstRx = nullptr;

    for (auto deviceSet : MainCore::instance()->getDeviceSets())
    {
        if (!deviceSet->m_deviceSourceEngine) {
            continue;
        }
        if (deviceSet->m_deviceAPI && (deviceSet->m_deviceAPI->getHardwareId() == m_hwType)) {
            return deviceSet;
        }
        if (!firstRx) {
            firstRx = deviceSet;
        }
    }

    return firstRx;
}

bool RBSController::rspAvailable()
{
    DeviceEnumerator *enumerator = DeviceEnumerator::instance();

    for (int i = 0; i < enumerator->getNbRxSamplingDevices(); i++)
    {
        if (enumerator->getRxSamplingDevice(i)->hardwareId == m_hwType) {
            return true;
        }
    }

    return false;
}

void RBSController::startMode(const RBSMode& mode)
{
    m_pollTimer.stop();
    m_steps.clear();
    m_mode = mode;
    setState(Starting, QString("Lancement de %1…").arg(mode.m_name));

    // 1. An Rx device set
    addStep("Préparation du récepteur",
        [this](QString& error) {
            DeviceSet *deviceSet = findRxDeviceSet();

            if (deviceSet)
            {
                m_deviceSet = deviceSet;
                return true;
            }

            m_deviceSetCountBefore = (int) MainCore::instance()->getDeviceSets().size();
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->instanceDeviceSetPost(0, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible de créer un récepteur : %1").arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this]() {
            if (getDeviceSet()) {
                return true;
            }

            const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

            if ((int) deviceSets.size() > m_deviceSetCountBefore)
            {
                m_deviceSet = deviceSets.back();
                return true;
            }

            return false;
        },
        5000, "Le récepteur n'a pas pu être créé");

    // 2. The RSP1B on it
    addStep("Ouverture du RSP1B",
        [this](QString& error) {
            DeviceSet *deviceSet = getDeviceSet();

            if (deviceSet && (deviceSet->m_deviceAPI->getHardwareId() == m_hwType)) {
                return true;
            }

            if (!rspAvailable())
            {
                error = "RSP1B introuvable. Vérifie qu'il est branché et qu'il n'est pas attaché à WSL "
                        "(usbipd detach), puis relance RBS.";
                return false;
            }

            // No init(): it would set empty name and serial strings, which the device lookup
            // compares with the real ones. Unset fields match any device.
            SWGSDRangel::SWGDeviceListItem query;
            query.setDirection(0);
            query.setIndex(-1);
            query.setSequence(-1);
            query.setDeviceStreamIndex(-1);
            query.setHwType(new QString(m_hwType));
            SWGSDRangel::SWGDeviceListItem response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->devicesetDevicePut(getDeviceSetIndex(), query, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible d'ouvrir le RSP1B : %1").arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this]() {
            DeviceSet *deviceSet = getDeviceSet();
            return deviceSet && (deviceSet->m_deviceAPI->getHardwareId() == m_hwType);
        },
        8000, "Le RSP1B ne répond pas. Ferme les autres logiciels qui l'utilisent (SDR++, SDRuno, SatDump).");

    // 3. Remove the decoder of the previous mode
    addStep("Retrait du décodeur précédent",
        [this](QString& error) {
            (void) error;
            int channelIndex = getChannelIndex();

            if (channelIndex >= 0)
            {
                SWGSDRangel::SWGSuccessResponse response;
                SWGSDRangel::SWGErrorResponse errorResponse;
                errorResponse.init();
                m_webAPIAdapterInterface->devicesetChannelDelete(getDeviceSetIndex(), channelIndex, response, errorResponse);
            }
            else
            {
                m_channelUID = 0;
            }

            return true;
        },
        [this]() {
            if (getChannelIndex() < 0)
            {
                m_channelUID = 0;
                return true;
            }

            return false;
        },
        5000, "Le décodeur précédent n'a pas pu être retiré");

    // 4. Device settings of the mode
    addStep("Réglage du RSP1B",
        [this](QString& error) {
            int deviceSetIndex = getDeviceSetIndex();

            if (deviceSetIndex < 0)
            {
                error = "Le récepteur a disparu";
                return false;
            }

            for (auto it = m_mode.m_device.constBegin(); it != m_mode.m_device.constEnd(); ++it)
            {
                bool ok;

                if (it.key() == "centerFrequency") {
                    ok = ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, it.value().toDouble());
                } else {
                    ok = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, it.key(), it.value().toInt());
                }

                if (!ok) {
                    qWarning("RBSController: could not set device setting %s", qPrintable(it.key()));
                }
            }

            return true;
        });

    // 5. The decoder channel
    addStep("Ajout du décodeur",
        [this](QString& error) {
            m_channelsBefore = channelUIDs();
            SWGSDRangel::SWGChannelSettings query;
            query.setDirection(0);
            query.setChannelType(new QString(m_mode.m_channelType));
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->devicesetChannelPost(getDeviceSetIndex(), query, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible d'ajouter le décodeur %1 : %2").arg(m_mode.m_channelType).arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this]() {
            for (auto uid : channelUIDs())
            {
                if (!m_channelsBefore.contains(uid))
                {
                    m_channelUID = uid;
                    return true;
                }
            }

            return false;
        },
        5000, "Le décodeur n'a pas pu être ajouté");

    // 6. Its settings
    addStep("Réglage du décodeur",
        [this](QString& error) {
            ChannelAPI *channel = getChannel();

            if (!channel)
            {
                error = "Le décodeur a disparu";
                return false;
            }

            ChannelWebAPIUtils::patchChannelSetting(channel, "inputFrequencyOffset", m_mode.m_channelOffset);

            for (auto it = m_mode.m_channelSettings.constBegin(); it != m_mode.m_channelSettings.constEnd(); ++it)
            {
                if (!ChannelWebAPIUtils::patchChannelSetting(channel, it.key(), it.value().toVariant())) {
                    qWarning("RBSController: could not set channel setting %s", qPrintable(it.key()));
                }
            }

            return true;
        });

    // 7. Run
    addStep("Démarrage",
        [this](QString& error) {
            (void) error;
            if (!isDeviceRunning()) {
                ChannelWebAPIUtils::run(getDeviceSetIndex());
            }
            return true;
        },
        [this]() {
            return isDeviceRunning();
        },
        8000, "Le RSP1B n'a pas démarré. Vérifie le service « SDRplay API Service ».");

    m_stepStarted = false;
    m_pollTimer.start();
    poll();
}

void RBSController::poll()
{
    while (!m_steps.isEmpty())
    {
        Step& step = m_steps.first();

        if (!m_stepStarted)
        {
            QString error;
            qDebug("RBSController::poll: %s", qPrintable(step.m_label));

            if (!step.m_action(error))
            {
                m_pollTimer.stop();
                m_steps.clear();
                setState(Error, error);
                return;
            }

            m_stepStarted = true;
            m_stepTimer.start();
        }

        if (step.m_done && !step.m_done())
        {
            if (m_stepTimer.elapsed() > step.m_timeoutMs)
            {
                QString error = step.m_timeoutError.isEmpty() ? QString("%1 : délai dépassé").arg(step.m_label) : step.m_timeoutError;
                m_pollTimer.stop();
                m_steps.clear();
                setState(Error, error);
            }

            return; // wait for the next poll
        }

        m_steps.removeFirst();
        m_stepStarted = false;
    }

    m_pollTimer.stop();
    setState(Running, QString("%1 en cours").arg(m_mode.m_name));
    emit modeStarted(m_mode);
}

void RBSController::startDevice()
{
    int deviceSetIndex = getDeviceSetIndex();

    if (deviceSetIndex >= 0) {
        ChannelWebAPIUtils::run(deviceSetIndex);
    }
}

void RBSController::stopDevice()
{
    int deviceSetIndex = getDeviceSetIndex();

    if (deviceSetIndex >= 0) {
        ChannelWebAPIUtils::stop(deviceSetIndex);
    }
}

bool RBSController::isDeviceRunning() const
{
    DeviceSet *deviceSet = getDeviceSet();
    return deviceSet && deviceSet->m_deviceAPI && (deviceSet->m_deviceAPI->state() == DeviceAPI::StRunning);
}

qint64 RBSController::getCenterFrequency() const
{
    DeviceSet *deviceSet = getDeviceSet();

    if (deviceSet && deviceSet->m_deviceAPI)
    {
        if (DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource()) {
            return (qint64) source->getCenterFrequency();
        }
    }

    return 0;
}

int RBSController::getSampleRate() const
{
    DeviceSet *deviceSet = getDeviceSet();

    if (deviceSet && deviceSet->m_deviceAPI)
    {
        if (DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource()) {
            return source->getSampleRate();
        }
    }

    return 0;
}

void RBSController::setTunedFrequency(qint64 frequency)
{
    int deviceSetIndex = getDeviceSetIndex();

    if (deviceSetIndex >= 0) {
        ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, (double) (frequency - m_mode.m_channelOffset));
    }
}

bool RBSController::setDeviceSetting(const QString& key, int value)
{
    int deviceSetIndex = getDeviceSetIndex();
    return (deviceSetIndex >= 0) && ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, key, value);
}

bool RBSController::getDeviceSetting(const QString& key, int& value) const
{
    int deviceSetIndex = getDeviceSetIndex();
    return (deviceSetIndex >= 0) && ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, key, value);
}
