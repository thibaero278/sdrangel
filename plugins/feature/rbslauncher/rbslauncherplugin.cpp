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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "rbslaunchergui.h"
#endif
#include "rbslauncher.h"
#include "rbslauncherplugin.h"

const PluginDescriptor RBSLauncherPlugin::m_pluginDescriptor = {
    RBSLauncher::m_featureId,
    QStringLiteral("RBS Launcher"),
    QStringLiteral("7.28.0"),
    QStringLiteral("(c) Thibaud (RBS)"),
    QStringLiteral("https://github.com/thibaero278/sdrangel"),
    true,
    QStringLiteral("https://github.com/thibaero278/sdrangel")
};

RBSLauncherPlugin::RBSLauncherPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& RBSLauncherPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RBSLauncherPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerFeature(RBSLauncher::m_featureIdURI, RBSLauncher::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* RBSLauncherPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* RBSLauncherPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return RBSLauncherGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* RBSLauncherPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new RBSLauncher(webAPIAdapterInterface);
}
