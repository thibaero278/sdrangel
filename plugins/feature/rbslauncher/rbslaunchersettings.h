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

#ifndef INCLUDE_FEATURE_RBSLAUNCHERSETTINGS_H_
#define INCLUDE_FEATURE_RBSLAUNCHERSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QStringList>

// Settings saved with the SDRangel configuration. The RBS window layout itself is kept in
// QSettings by RBSWindow, so that it survives whatever configuration is loaded.
struct RBSLauncherSettings
{
    QString m_title;
    quint32 m_rgbColor;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    QString m_lastModeId;   //!< Mode started last, restarted when the window opens
    bool m_openOnStart;     //!< Open the RBS window as soon as the feature is created

    RBSLauncherSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RBSLauncherSettings& settings);
};

#endif // INCLUDE_FEATURE_RBSLAUNCHERSETTINGS_H_
