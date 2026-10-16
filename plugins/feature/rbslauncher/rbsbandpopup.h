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

#ifndef INCLUDE_FEATURE_RBSBANDPOPUP_H_
#define INCLUDE_FEATURE_RBSBANDPOPUP_H_

#include <QFrame>

#include "rbsmode.h"

class QLabel;
class QLineEdit;
class QListWidget;

// Drop down of the "Bande" button: the bands of the mode on the left, their known frequencies on
// the right, and at the bottom a frequency to type in kHz and steps of 1 and 10 kHz
class RBSBandPopup : public QFrame
{
    Q_OBJECT
public:
    explicit RBSBandPopup(QWidget *parent = nullptr);

    //! Show under the anchor widget for the mode, the band of frequency selected
    void showFor(QWidget *anchor, const RBSMode& mode, qint64 frequency);

    static QString formatMHz(qint64 frequency);  //!< "14,074 MHz", "1 090 MHz"

signals:
    void frequencyChosen(qint64 frequency);

private:
    RBSMode m_mode;
    qint64 m_frequency;
    QLabel *m_bandsTitle;
    QListWidget *m_bands;
    QListWidget *m_frequencies;
    QLineEdit *m_entry;

    void showBand(const QString& band);
    void choose(qint64 frequency, bool close);
};

#endif // INCLUDE_FEATURE_RBSBANDPOPUP_H_
