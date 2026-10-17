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

#ifndef INCLUDE_FEATURE_RBSMODE_H_
#define INCLUDE_FEATURE_RBSMODE_H_

#include <QJsonObject>
#include <QList>
#include <QString>

// One entry of the mode list: everything needed to set up the receiver for a use,
// read from a JSON file ("fiche de mode")
struct RBSMode
{
    // A known frequency for the mode, offered by the band selector
    struct Frequency
    {
        QString m_band;           //!< Band it is listed under, e.g. "20 m HF"
        qint64 m_frequency = 0;   //!< Tuned (decoder) frequency in Hz
        QString m_note;           //!< e.g. "monde entier", "Europe"
        bool m_standard = false;  //!< The usual one of the band
    };

    QString m_id;
    int m_order = 100;        //!< Position in the mode list
    QString m_name;
    QString m_group;
    QString m_band;
    QString m_view;           //!< Decode view: "ft8", "aprs", "adsb" or "cw"
    QString m_demod;          //!< Label shown in the Radio section (USB, FM...)
    QString m_listen;         //!< Listening demodulator used until the user picks one ("" = none)
    QString m_antenna;
    QString m_description;    //!< What the mode is, for the decoder panel
    QString m_tip;            //!< Practical advice, highlighted in the decoder panel
    QString m_tag;            //!< Short tag shown in the mode list (IMAGE...), may be empty
    QJsonObject m_device;     //!< SDRplayV3 settings keys and values
    QString m_channelType;    //!< SDRangel channel id, e.g. "FT8Demod"
    int m_channelOffset = 0;  //!< Channel inputFrequencyOffset in Hz
    QJsonObject m_channelSettings;
    QString m_featureType;    //!< SDRangel feature decoding the channel's audio, e.g. "MorseDecoder" ("" = none)
    QString m_featureURI;     //!< Its URI, e.g. "sdrangel.feature.morsedecoder"
    QJsonObject m_featureSettings;
    QList<Frequency> m_frequencies;
    int m_fftSize = 4096;                 //!< Spectrum resolution
    int m_waterfallLinesPerSecond = 12;   //!< Waterfall speed
    int m_maxLevel = -30;                 //!< Top of the spectrum scale, dB
    int m_minLevel = -110;                //!< Bottom of the spectrum scale, dB (the noise should sit just above)
    // Decoder passband drawn on the spectrum, relative to the channel frequency
    QString m_markerSideband;  //!< "usb", "lsb", "dsb" or "" (no marker)
    int m_markerLow = 0;       //!< Low edge in Hz (usb / lsb)
    int m_markerHigh = 0;      //!< High edge in Hz (usb / lsb), half width (dsb)
    bool m_tunable = false;    //!< A click on the spectrum moves the decoder there
    int m_tuneShift = 0;       //!< Shown frequency minus channel frequency: the CW tone pitch, so that the
                               //!< frequency shown is the one of the signal, as on a CW transceiver

    qint64 centerFrequency() const { return (qint64) m_device.value("centerFrequency").toDouble(); }
    qint64 tunedFrequency() const { return centerFrequency() + m_channelOffset + m_tuneShift; }
    //! Band of the known frequency closest to frequency (within 150 kHz), else HF / VHF / UHF
    QString bandOf(qint64 frequency) const;

    static bool fromJson(const QJsonObject& json, RBSMode& mode);

    //! Built-in modes from the plugin resources, overridden or extended by the JSON files
    //! found in the user's modes folder (same id = override)
    static QList<RBSMode> loadAll();
    static QString userModesPath();
};

#endif // INCLUDE_FEATURE_RBSMODE_H_
