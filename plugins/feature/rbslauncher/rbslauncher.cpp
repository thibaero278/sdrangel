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

#include "rbslauncher.h"

MESSAGE_CLASS_DEFINITION(RBSLauncher::MsgConfigureRBSLauncher, Message)

const char* const RBSLauncher::m_featureIdURI = "sdrangel.feature.rbslauncher";
const char* const RBSLauncher::m_featureId = "RBSLauncher";

RBSLauncher::RBSLauncher(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    setObjectName(m_featureId);
    setState(StIdle);
}

bool RBSLauncher::handleMessage(const Message& cmd)
{
    if (MsgConfigureRBSLauncher::match(cmd))
    {
        const MsgConfigureRBSLauncher& cfg = (const MsgConfigureRBSLauncher&) cmd;
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }

    return false;
}

QByteArray RBSLauncher::serialize() const
{
    return m_settings.serialize();
}

bool RBSLauncher::deserialize(const QByteArray& data)
{
    bool ok = m_settings.deserialize(data);

    if (!ok) {
        m_settings.resetToDefaults();
    }

    m_inputMessageQueue.push(MsgConfigureRBSLauncher::create(m_settings, QStringList(), true));
    return ok;
}

void RBSLauncher::applySettings(const RBSLauncherSettings& settings, const QStringList& settingsKeys, bool force)
{
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
