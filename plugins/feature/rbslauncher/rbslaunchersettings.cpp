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

#include <QColor>

#include "util/simpleserializer.h"

#include "rbslaunchersettings.h"

RBSLauncherSettings::RBSLauncherSettings()
{
    resetToDefaults();
}

void RBSLauncherSettings::resetToDefaults()
{
    m_title = "RBS Launcher";
    m_rgbColor = QColor(61, 139, 255).rgb();
    m_workspaceIndex = 0;
    m_lastModeId = "ft8_20m";
    m_openOnStart = true;
}

QByteArray RBSLauncherSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, m_workspaceIndex);
    s.writeBlob(4, m_geometryBytes);
    s.writeString(5, m_lastModeId);
    s.writeBool(6, m_openOnStart);

    return s.final();
}

bool RBSLauncherSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_title, "RBS Launcher");
        d.readU32(2, &m_rgbColor, QColor(61, 139, 255).rgb());
        d.readS32(3, &m_workspaceIndex, 0);
        d.readBlob(4, &m_geometryBytes);
        d.readString(5, &m_lastModeId, "ft8_20m");
        d.readBool(6, &m_openOnStart, true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RBSLauncherSettings::applySettings(const QStringList& settingsKeys, const RBSLauncherSettings& settings)
{
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("lastModeId")) {
        m_lastModeId = settings.m_lastModeId;
    }
    if (settingsKeys.contains("openOnStart")) {
        m_openOnStart = settings.m_openOnStart;
    }
}
