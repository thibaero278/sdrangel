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

#ifndef INCLUDE_FEATURE_RBSLAUNCHER_H_
#define INCLUDE_FEATURE_RBSLAUNCHER_H_

#include "feature/feature.h"
#include "util/message.h"

#include "rbslaunchersettings.h"

class WebAPIAdapterInterface;

// The feature side holds the settings only: all the work is done by the GUI, which drives
// SDRangel through the Web API adapter in the main thread
class RBSLauncher : public Feature
{
    Q_OBJECT
public:
    class MsgConfigureRBSLauncher : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RBSLauncherSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRBSLauncher* create(const RBSLauncherSettings& settings, const QStringList& settingsKeys, bool force) {
            return new MsgConfigureRBSLauncher(settings, settingsKeys, force);
        }

    private:
        RBSLauncherSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureRBSLauncher(const RBSLauncherSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    RBSLauncher(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~RBSLauncher() override = default;

    void destroy() override { delete this; }
    bool handleMessage(const Message& cmd) override;

    void getIdentifier(QString& id) const override { id = objectName(); }
    QString getIdentifier() const override { return objectName(); }
    void getTitle(QString& title) const override { title = m_settings.m_title; }

    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;

    const RBSLauncherSettings& getSettings() const { return m_settings; }

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    RBSLauncherSettings m_settings;

    void applySettings(const RBSLauncherSettings& settings, const QStringList& settingsKeys, bool force = false);
};

#endif // INCLUDE_FEATURE_RBSLAUNCHER_H_
