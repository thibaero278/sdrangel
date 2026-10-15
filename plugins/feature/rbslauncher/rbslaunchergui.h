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

#ifndef INCLUDE_FEATURE_RBSLAUNCHERGUI_H_
#define INCLUDE_FEATURE_RBSLAUNCHERGUI_H_

#include <QPointer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"

#include "rbslaunchersettings.h"

class PluginAPI;
class FeatureUISet;
class Feature;
class RBSLauncher;
class RBSWindow;
class QCheckBox;

// Small panel in the SDRangel workspace. Its job is to own the RBS window: it opens it when
// the feature is created (so adding RBS Launcher once, and saving the configuration, makes RBS
// open at every start) and brings it back when it has been closed.
class RBSLauncherGUI : public FeatureGUI
{
    Q_OBJECT
public:
    static RBSLauncherGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    void destroy() override;

    void resetToDefaults() override;
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    MessageQueue *getInputMessageQueue() override { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) override;
    int getWorkspaceIndex() const override { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) override { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const override { return m_settings.m_geometryBytes; }

private:
    RBSLauncher *m_rbsLauncher;
    RBSLauncherSettings m_settings;
    MessageQueue m_inputMessageQueue;
    QPointer<RBSWindow> m_window;
    QCheckBox *m_openOnStart;
    bool m_windowShownOnce;

    explicit RBSLauncherGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    ~RBSLauncherGUI() override;

    bool handleMessage(const Message& message);
    void displaySettings();
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void openWindow();

private slots:
    void handleInputMessages();
    void onMenuDialogCalled(const QPoint& p);
};

#endif // INCLUDE_FEATURE_RBSLAUNCHERGUI_H_
