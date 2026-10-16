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

#include <algorithm>
#include <cstdlib>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include "rbsmode.h"

bool RBSMode::fromJson(const QJsonObject& json, RBSMode& mode)
{
    mode.m_id = json.value("id").toString();
    mode.m_order = json.value("order").toInt(100);
    mode.m_name = json.value("name").toString(mode.m_id);
    mode.m_group = json.value("group").toString("Modes");
    mode.m_band = json.value("band").toString();
    mode.m_view = json.value("view").toString();
    mode.m_demod = json.value("demod").toString();
    mode.m_antenna = json.value("antenna").toString();
    mode.m_device = json.value("device").toObject();

    QJsonObject channel = json.value("channel").toObject();
    mode.m_channelType = channel.value("type").toString();
    mode.m_channelOffset = channel.value("inputFrequencyOffset").toInt(0);
    mode.m_channelSettings = channel.value("settings").toObject();

    const QJsonArray frequencies = json.value("frequencies").toArray();

    for (const auto& value : frequencies)
    {
        QJsonObject object = value.toObject();
        Frequency frequency;
        frequency.m_band = object.value("band").toString(mode.m_band);
        frequency.m_frequency = (qint64) object.value("frequency").toDouble();
        frequency.m_note = object.value("note").toString();
        frequency.m_standard = object.value("standard").toBool(false);

        if (frequency.m_frequency > 0) {
            mode.m_frequencies.append(frequency);
        }
    }

    QJsonObject spectrum = json.value("spectrum").toObject();
    mode.m_fftSize = spectrum.value("fftSize").toInt(mode.m_fftSize);
    mode.m_waterfallLinesPerSecond = spectrum.value("linesPerSecond").toInt(mode.m_waterfallLinesPerSecond);
    mode.m_maxLevel = spectrum.value("max").toInt(mode.m_maxLevel);
    mode.m_minLevel = spectrum.value("min").toInt(mode.m_minLevel);

    return !mode.m_id.isEmpty() && !mode.m_channelType.isEmpty() && mode.m_device.contains("centerFrequency");
}

QString RBSMode::bandOf(qint64 frequency) const
{
    const Frequency *closest = nullptr;

    for (const auto& known : m_frequencies)
    {
        if ((std::abs(known.m_frequency - frequency) <= 150000)
            && (!closest || (std::abs(known.m_frequency - frequency) < std::abs(closest->m_frequency - frequency)))) {
            closest = &known;
        }
    }

    if (closest) {
        return closest->m_band;
    } else if (frequency < 30000000) {
        return "HF";
    } else if (frequency < 300000000) {
        return "VHF";
    } else {
        return "UHF";
    }
}

QString RBSMode::userModesPath()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return dir.absoluteFilePath("rbs/modes");
}

static void loadDir(const QString& path, QList<RBSMode>& modes)
{
    QDir dir(path);
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

    for (const auto& fileName : files)
    {
        QFile file(dir.absoluteFilePath(fileName));

        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        RBSMode mode;

        if (doc.isObject() && RBSMode::fromJson(doc.object(), mode))
        {
            int existing = -1;

            for (int i = 0; i < modes.size(); i++)
            {
                if (modes[i].m_id == mode.m_id) {
                    existing = i;
                }
            }

            if (existing >= 0) {
                modes[existing] = mode;
            } else {
                modes.append(mode);
            }
        }
        else
        {
            qWarning("RBSMode: ignoring %s: %s", qPrintable(file.fileName()), qPrintable(error.errorString()));
        }
    }
}

QList<RBSMode> RBSMode::loadAll()
{
    QList<RBSMode> modes;
    loadDir(":/rbslauncher/modes", modes);
    loadDir(userModesPath(), modes);
    std::stable_sort(modes.begin(), modes.end(), [](const RBSMode& a, const RBSMode& b) {
        return a.m_order < b.m_order;
    });
    return modes;
}
