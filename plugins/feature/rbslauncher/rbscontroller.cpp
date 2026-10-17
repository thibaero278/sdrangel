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
#include "SWGFeatureActions.h"
#include "SWGFeatureSettings.h"
#include "SWGMorseDecoderActions.h"
#include "SWGSuccessResponse.h"

#include "channel/channelapi.h"
#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "device/deviceenumerator.h"
#include "device/deviceset.h"
#include "dsp/devicesamplesource.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "pipes/objectpipe.h"
#include "webapi/webapiadapterinterface.h"
#include "maincore.h"

#include "rbscontroller.h"

const char* const RBSController::m_sdrplayHwType = "SDRplayV3";
const char* const RBSController::m_channelTitle = "RBS";
const char* const RBSController::m_listenTitle = "RBS écoute";
const char* const RBSController::m_featureTitle = "RBS décodeur";

RBSController::RBSController(WebAPIAdapterInterface *webAPIAdapterInterface, QObject *parent) :
    QObject(parent),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_hwType(m_sdrplayHwType),
    m_sequence(-1),
    m_state(Idle),
    m_stepStarted(false),
    m_modeStartPending(false),
    m_deviceSet(nullptr),
    m_channelUID(0),
    m_listenUID(0),
    m_featureUID(0),
    m_channelOffset(0),
    m_featureSelectRetried(false),
    m_deviceSetCountBefore(0)
{
    m_pollTimer.setInterval(100);
    connect(&m_pollTimer, &QTimer::timeout, this, &RBSController::poll);
}

void RBSController::setDevice(const QString& hwType, int sequence)
{
    m_hwType = hwType;
    m_sequence = sequence;
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

void RBSController::runSteps()
{
    if (!m_pollTimer.isActive())
    {
        m_stepStarted = false;
        m_pollTimer.start();
        poll();
    }
}

// ---------------------------------------------------------------------------
// Device set and channels lookup

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

bool RBSController::hasDevice() const
{
    DeviceSet *deviceSet = getDeviceSet();
    return deviceSet && deviceSet->m_deviceAPI && (deviceSet->m_deviceAPI->getHardwareId() == m_hwType);
}

int RBSController::channelIndex(uint64_t uid) const
{
    DeviceSet *deviceSet = getDeviceSet();

    if (!deviceSet || (uid == 0)) {
        return -1;
    }

    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = deviceSet->getChannelAt(i);

        if (channel && (channel->getUID() == uid)) {
            return i;
        }
    }

    return -1;
}

ChannelAPI *RBSController::channel(uint64_t uid) const
{
    int index = channelIndex(uid);
    return index < 0 ? nullptr : getDeviceSet()->getChannelAt(index);
}

ChannelAPI *RBSController::getChannel() const
{
    return channel(m_channelUID);
}

int RBSController::getChannelIndex() const
{
    return channelIndex(m_channelUID);
}

int RBSController::getListenChannelIndex() const
{
    return channelIndex(m_listenUID);
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

// Channels RBS added, in this session or a previous one (they are saved with the configuration),
// recognised by their title, in increasing index order
QList<int> RBSController::rbsChannelIndexes() const
{
    QList<int> indexes;
    int deviceSetIndex = getDeviceSetIndex();
    DeviceSet *deviceSet = getDeviceSet();

    if (!deviceSet) {
        return indexes;
    }

    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
    {
        QString title;

        if (ChannelWebAPIUtils::getChannelSetting(deviceSetIndex, i, "title", title) && title.startsWith(m_channelTitle)) {
            indexes.append(i);
        }
    }

    return indexes;
}

// Features (Morse decoder...) live in the single feature set of the GUI, as the RBS Launcher itself
static FeatureSet *featureSet()
{
    std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();
    return featureSets.empty() ? nullptr : featureSets[0];
}

QSet<uint64_t> RBSController::featureUIDs() const
{
    QSet<uint64_t> uids;

    if (FeatureSet *features = featureSet())
    {
        for (int i = 0; i < features->getNumberOfFeatures(); i++)
        {
            if (Feature *feature = features->getFeatureAt(i)) {
                uids.insert(feature->getUID());
            }
        }
    }

    return uids;
}

int RBSController::featureIndex(uint64_t uid) const
{
    FeatureSet *features = featureSet();

    if (!features || (uid == 0)) {
        return -1;
    }

    for (int i = 0; i < features->getNumberOfFeatures(); i++)
    {
        Feature *feature = features->getFeatureAt(i);

        if (feature && (feature->getUID() == uid)) {
            return i;
        }
    }

    return -1;
}

// Decoder features RBS added, recognised by their title, in increasing index order
QList<int> RBSController::rbsFeatureIndexes() const
{
    QList<int> indexes;
    FeatureSet *features = featureSet();

    if (!features) {
        return indexes;
    }

    for (int i = 0; i < features->getNumberOfFeatures(); i++)
    {
        QString title;

        if (ChannelWebAPIUtils::getFeatureSetting(0, i, "title", title) && (title == m_featureTitle)) {
            indexes.append(i);
        }
    }

    return indexes;
}

// The decoder feature takes the demodulated audio of our decoder channel through a "demod" data pipe
bool RBSController::featureSelectedChannel() const
{
    ChannelAPI *decoder = getChannel();
    int index = featureIndex(m_featureUID);

    if (!decoder || (index < 0)) {
        return false;
    }

    const Feature *feature = featureSet()->getFeatureAt(index);
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getDataPipes().getDataPipes(decoder, "demod", pipes);

    for (const auto pipe : pipes)
    {
        if (pipe->m_consumer == feature) {
            return true;
        }
    }

    return false;
}

bool RBSController::selectFeatureChannel()
{
    int index = featureIndex(m_featureUID);

    if ((index < 0) || (m_mode.m_featureType != "MorseDecoder")) {
        return false;
    }

    SWGSDRangel::SWGFeatureActions query;
    query.setFeatureType(new QString(m_mode.m_featureType));
    SWGSDRangel::SWGMorseDecoderActions *actions = new SWGSDRangel::SWGMorseDecoderActions();
    actions->setDeviceId(getDeviceSetIndex());
    actions->setChannelId(getChannelIndex());
    query.setMorseDecoderActions(actions);
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse errorResponse;
    errorResponse.init();
    int rc = m_webAPIAdapterInterface->featuresetFeatureActionsPost(0, index, {"deviceId", "channelId"}, query, response, errorResponse);

    if (rc / 100 != 2) {
        qWarning("RBSController::selectFeatureChannel: %s", qPrintable(*errorResponse.getMessage()));
    }

    return rc / 100 == 2;
}

// The decoder feature of the mode (Morse decoder): added, titled, set up, fed with the audio of the
// decoder channel and started
void RBSController::addFeatureSteps()
{
    addStep(QString("Ajout du décodeur %1").arg(m_mode.m_featureType),
        [this](QString& error) {
            m_featuresBefore = featureUIDs();
            SWGSDRangel::SWGFeatureSettings query;
            query.setFeatureType(new QString(m_mode.m_featureType));
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->featuresetFeaturePost(0, query, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible d'ajouter le décodeur %1 : %2").arg(m_mode.m_featureType).arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this]() {
            for (auto uid : featureUIDs())
            {
                if (!m_featuresBefore.contains(uid))
                {
                    m_featureUID = uid;
                    return true;
                }
            }

            return false;
        },
        5000, QString("Le décodeur %1 n'a pas pu être ajouté").arg(m_mode.m_featureType));

    addStep("Réglage du décodeur Morse",
        [this](QString& error) {
            int index = featureIndex(m_featureUID);

            if (index < 0)
            {
                error = "Le décodeur a disparu";
                return false;
            }

            ChannelWebAPIUtils::patchFeatureSetting(0, index, "title", QString(m_featureTitle));

            for (auto it = m_mode.m_featureSettings.constBegin(); it != m_mode.m_featureSettings.constEnd(); ++it)
            {
                bool ok = it.value().isString() ?
                    ChannelWebAPIUtils::patchFeatureSetting(0, index, it.key(), it.value().toString()) :
                    ChannelWebAPIUtils::patchFeatureSetting(0, index, it.key(), it.value().toDouble());

                if (!ok) {
                    qWarning("RBSController: could not set feature setting %s", qPrintable(it.key()));
                }
            }

            m_featureSelectRetried = false;
            selectFeatureChannel();
            ChannelWebAPIUtils::runFeature(0, index);
            return true;
        },
        [this]() {
            if (featureSelectedChannel()) {
                return true;
            }

            // The feature may not have seen the channel yet when it was asked: ask again once
            if (!m_featureSelectRetried && (m_stepTimer.elapsed() > 1500))
            {
                m_featureSelectRetried = true;
                selectFeatureChannel();
            }

            return false;
        },
        6000, "Le décodeur Morse ne reçoit pas le son du canal");
}

// The device set already used, else the first Rx one, preferring one that has the chosen device
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

bool RBSController::deviceAvailable() const
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

QString RBSController::deviceName() const
{
    return isSDRplay() ? QString("RSP1B") : m_hwType;
}

// ---------------------------------------------------------------------------
// Steps

void RBSController::addChannelSteps(const QString& label, const QString& channelType, uint64_t *uid)
{
    addStep(label,
        [this, channelType](QString& error) {
            m_channelsBefore = channelUIDs();
            SWGSDRangel::SWGChannelSettings query;
            query.setDirection(0);
            query.setChannelType(new QString(channelType));
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->devicesetChannelPost(getDeviceSetIndex(), query, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible d'ajouter le canal %1 : %2").arg(channelType).arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this, uid]() {
            for (auto channelUID : channelUIDs())
            {
                if (!m_channelsBefore.contains(channelUID))
                {
                    *uid = channelUID;
                    return true;
                }
            }

            return false;
        },
        5000, QString("%1 : le canal n'a pas pu être ajouté").arg(label));
}

void RBSController::startMode(const RBSMode& mode)
{
    m_pollTimer.stop();
    m_steps.clear();
    m_mode = mode;
    // A sound card gives the audio as it is: the decoder at its centre, no frequency to set
    m_channelOffset = isAudioSource() ? 0 : mode.m_channelOffset;
    m_modeStartPending = true;
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

    // 2. Before changing the device, stop the current one and give it a second: closing the
    // SDRplay API while it is still starting can crash it
    addStep("Arrêt de l'appareil précédent",
        [this](QString& error) {
            (void) error;
            if (!hasDevice() && isDeviceRunning()) {
                stopDevice();
            }
            return true;
        },
        [this]() {
            return hasDevice() || (!isDeviceRunning() && (m_stepTimer.elapsed() > 1000));
        },
        10000, "L'appareil précédent ne s'arrête pas");

    // 3. The chosen device on it
    addStep("Ouverture de l'appareil",
        [this](QString& error) {
            if (hasDevice()) {
                return true;
            }

            if (!deviceAvailable())
            {
                error = isSDRplay() ?
                    QString("RSP1B introuvable. Vérifie qu'il est branché, qu'aucun autre logiciel ne l'utilise "
                            "et qu'il n'est pas attaché à WSL (usbipd detach), puis relance RBS.") :
                    QString("Source %1 introuvable").arg(m_hwType);
                return false;
            }

            // No init(): it would set empty name and serial strings, which the device lookup
            // compares with the real ones. Unset fields match any device.
            SWGSDRangel::SWGDeviceListItem query;
            query.setDirection(0);
            query.setIndex(-1);
            query.setSequence(m_sequence);
            query.setDeviceStreamIndex(-1);
            query.setHwType(new QString(m_hwType));
            SWGSDRangel::SWGDeviceListItem response;
            SWGSDRangel::SWGErrorResponse errorResponse;
            errorResponse.init();
            int rc = m_webAPIAdapterInterface->devicesetDevicePut(getDeviceSetIndex(), query, response, errorResponse);

            if (rc / 100 != 2)
            {
                error = QString("Impossible d'ouvrir %1 : %2").arg(deviceName()).arg(*errorResponse.getMessage());
                return false;
            }

            return true;
        },
        [this]() {
            return hasDevice();
        },
        8000, QString("%1 ne répond pas. Ferme les autres logiciels qui l'utilisent (SDR++, SDRuno, SatDump).").arg(deviceName()));

    // 4. Remove the channels and decoder features RBS added before (decoder and listening)
    addStep("Retrait des canaux précédents",
        [this](QString& error) {
            (void) error;
            int deviceSetIndex = getDeviceSetIndex();
            QList<int> rbsChannels = rbsChannelIndexes();
            QList<int> rbsFeatures = rbsFeatureIndexes();

            // From the highest index, as deleting a channel renumbers the ones after it.
            // Features first: they stop taking the channel audio
            for (int i = rbsFeatures.size() - 1; i >= 0; i--)
            {
                SWGSDRangel::SWGSuccessResponse response;
                SWGSDRangel::SWGErrorResponse errorResponse;
                errorResponse.init();
                m_webAPIAdapterInterface->featuresetFeatureDelete(0, rbsFeatures[i], response, errorResponse);
            }

            for (int i = rbsChannels.size() - 1; i >= 0; i--)
            {
                SWGSDRangel::SWGSuccessResponse response;
                SWGSDRangel::SWGErrorResponse errorResponse;
                errorResponse.init();
                m_webAPIAdapterInterface->devicesetChannelDelete(deviceSetIndex, rbsChannels[i], response, errorResponse);
            }

            return true;
        },
        [this]() {
            if (rbsChannelIndexes().isEmpty() && rbsFeatureIndexes().isEmpty())
            {
                m_channelUID = 0;
                m_listenUID = 0;
                m_featureUID = 0;
                return true;
            }

            return false;
        },
        5000, "Les canaux précédents n'ont pas pu être retirés");

    // 5. Device settings of the mode. They are written for the SDRplay; another source only gets
    // the frequency
    addStep("Réglage de l'appareil",
        [this](QString& error) {
            int deviceSetIndex = getDeviceSetIndex();

            if (deviceSetIndex < 0)
            {
                error = "Le récepteur a disparu";
                return false;
            }

            for (auto it = m_mode.m_device.constBegin(); it != m_mode.m_device.constEnd(); ++it)
            {
                if (it.key() == "centerFrequency")
                {
                    if (!isAudioSource()) {
                        ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, it.value().toDouble());
                    }
                }
                else if (isSDRplay() && !ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, it.key(), it.value().toInt()))
                {
                    qWarning("RBSController: could not set device setting %s", qPrintable(it.key()));
                }
            }

            return true;
        });

    // 6. The decoder channel and its settings
    addChannelSteps("Ajout du décodeur", m_mode.m_channelType, &m_channelUID);
    addStep("Réglage du décodeur",
        [this](QString& error) {
            ChannelAPI *decoder = getChannel();

            if (!decoder)
            {
                error = "Le décodeur a disparu";
                return false;
            }

            ChannelWebAPIUtils::patchChannelSetting(decoder, "title", QString(m_channelTitle));
            ChannelWebAPIUtils::patchChannelSetting(decoder, "inputFrequencyOffset", m_channelOffset);

            for (auto it = m_mode.m_channelSettings.constBegin(); it != m_mode.m_channelSettings.constEnd(); ++it)
            {
                if (!ChannelWebAPIUtils::patchChannelSetting(decoder, it.key(), it.value().toVariant())) {
                    qWarning("RBSController: could not set channel setting %s", qPrintable(it.key()));
                }
            }

            // Morse from a sound card (WebSDR, recording): the tone can be anywhere in the audio,
            // the whole of it goes to the decoder, which finds the tone itself
            if (isAudioSource() && (m_mode.m_view == "cw"))
            {
                ChannelWebAPIUtils::patchChannelSetting(decoder, "rfBandwidth", 3000);
                ChannelWebAPIUtils::patchChannelSetting(decoder, "lowCutoff", 100);
            }

            return true;
        });

    // 7. The feature that decodes the channel audio, if the mode has one (Morse decoder)
    if (!m_mode.m_featureType.isEmpty()) {
        addFeatureSteps();
    }

    // 8. Run
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
        20000, QString("%1 n'a pas démarré. Vérifie le service « SDRplay API Service », ou débranche et rebranche le RSP1B.").arg(deviceName()));

    runSteps();
}

void RBSController::setListen(const Listen& listen)
{
    // Remove the current listening channel
    addStep("Retrait de l'écoute",
        [this](QString& error) {
            (void) error;
            int index = getListenChannelIndex();

            if (index >= 0)
            {
                SWGSDRangel::SWGSuccessResponse response;
                SWGSDRangel::SWGErrorResponse errorResponse;
                errorResponse.init();
                m_webAPIAdapterInterface->devicesetChannelDelete(getDeviceSetIndex(), index, response, errorResponse);
            }

            return true;
        },
        [this]() {
            if (getListenChannelIndex() < 0)
            {
                m_listenUID = 0;
                return true;
            }

            return false;
        },
        5000, "L'écoute précédente n'a pas pu être retirée");

    QString channelType = listenChannelType(listen.m_demod);

    if (channelType.isEmpty())
    {
        runSteps();
        return;
    }

    addChannelSteps(QString("Écoute %1").arg(listen.m_demod), channelType, &m_listenUID);
    addStep("Réglage de l'écoute",
        [this, listen](QString& error) {
            ChannelAPI *listenChannel = channel(m_listenUID);

            if (!listenChannel)
            {
                error = "Le canal d'écoute a disparu";
                return false;
            }

            ChannelWebAPIUtils::patchChannelSetting(listenChannel, "title", QString(m_listenTitle));
            ChannelWebAPIUtils::patchChannelSetting(listenChannel, "inputFrequencyOffset", m_channelOffset);
            ChannelWebAPIUtils::patchChannelSetting(listenChannel, "audioMute", 0);
            applyListenSettings(listen, true);
            return true;
        });

    runSteps();
}

void RBSController::updateListen(const Listen& listen)
{
    if (channel(m_listenUID)) {
        applyListenSettings(listen, false);
    }
}

// Volume: 50 % is the demodulator's own default level
void RBSController::applyListenSettings(const Listen& listen, bool all)
{
    ChannelAPI *listenChannel = channel(m_listenUID);

    if (!listenChannel) {
        return;
    }

    const QString& demod = listen.m_demod;
    int bandwidth = listen.m_bandwidth > 0 ? listen.m_bandwidth : defaultBandwidth(demod);
    double volumeScale = listen.m_volume / 50.0;

    if ((demod == "USB") || (demod == "LSB") || (demod == "CW"))
    {
        // SSB demodulator: the passband is lowCutoff..rfBandwidth, negative for LSB
        int lowCutoff = demod == "CW" ? 300 : 200;
        int sign = demod == "LSB" ? -1 : 1;
        int high = demod == "CW" ? lowCutoff + bandwidth : bandwidth;
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "rfBandwidth", sign * high);
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "lowCutoff", sign * lowCutoff);
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "volume", 1.0 * volumeScale);

        if (all) {
            ChannelWebAPIUtils::patchChannelSetting(listenChannel, "agc", 1);
        }
    }
    else
    {
        double defaultVolume = demod == "NFM" ? 1.0 : 2.0;
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "rfBandwidth", bandwidth);
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "squelch", (double) listen.m_squelch);
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "volume", defaultVolume * volumeScale);
    }
}

QString RBSController::listenChannelType(const QString& demod)
{
    if (demod == "NFM") {
        return "NFMDemod";
    } else if (demod == "AM") {
        return "AMDemod";
    } else if ((demod == "USB") || (demod == "LSB") || (demod == "CW")) {
        return "SSBDemod";
    } else if (demod == "WFM") {
        return "WFMDemod";
    }

    return QString();
}

int RBSController::defaultBandwidth(const QString& demod)
{
    if (demod == "NFM") {
        return 12500;
    } else if (demod == "AM") {
        return 8000;
    } else if (demod == "WFM") {
        return 150000;
    } else if (demod == "CW") {
        return 500;
    }

    return 2700; // USB, LSB
}

bool RBSController::hasSquelch(const QString& demod)
{
    return (demod == "NFM") || (demod == "AM") || (demod == "WFM");
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
                m_modeStartPending = false;
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
                m_modeStartPending = false;
                setState(Error, error);
            }

            return; // wait for the next poll
        }

        m_steps.removeFirst();
        m_stepStarted = false;
    }

    m_pollTimer.stop();

    if (m_modeStartPending)
    {
        m_modeStartPending = false;
        setState(Running, QString("%1 en cours").arg(m_mode.m_name));
        emit modeStarted(m_mode);
    }
}

// ---------------------------------------------------------------------------
// Device

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
        ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, (double) (frequency - m_channelOffset - m_mode.m_tuneShift));
    }
}

void RBSController::setChannelOffset(int offset)
{
    m_channelOffset = offset;

    if (ChannelAPI *decoder = getChannel()) {
        ChannelWebAPIUtils::patchChannelSetting(decoder, "inputFrequencyOffset", offset);
    }
    if (ChannelAPI *listenChannel = channel(m_listenUID)) {
        ChannelWebAPIUtils::patchChannelSetting(listenChannel, "inputFrequencyOffset", offset);
    }
}

bool RBSController::setDeviceSetting(const QString& key, int value)
{
    int deviceSetIndex = getDeviceSetIndex();
    return (deviceSetIndex >= 0) && ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, key, value);
}

bool RBSController::setFeatureSetting(const QString& key, double value)
{
    int index = featureIndex(m_featureUID);
    return (index >= 0) && ChannelWebAPIUtils::patchFeatureSetting(0, index, key, value);
}

bool RBSController::getDeviceSetting(const QString& key, int& value) const
{
    int deviceSetIndex = getDeviceSetIndex();
    return (deviceSetIndex >= 0) && ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, key, value);
}
