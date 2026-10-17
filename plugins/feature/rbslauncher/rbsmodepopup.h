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

#ifndef INCLUDE_FEATURE_RBSMODEPOPUP_H_
#define INCLUDE_FEATURE_RBSMODEPOPUP_H_

#include <QFrame>
#include <QList>

#include "rbsmode.h"

class QLineEdit;
class QListWidget;

// Drop down of the "Mode" button, as the MultiPSK Deck: categories on the left with their number
// of modes, the modes of the category on the right with their number of known frequencies,
// a search over all of them on top
class RBSModePopup : public QFrame
{
    Q_OBJECT
public:
    explicit RBSModePopup(QWidget *parent = nullptr);

    void showFor(QWidget *anchor, const QList<RBSMode>& modes, const QString& currentId);

    static const QStringList& categories();              //!< In display order
    static QString categoryOf(const RBSMode& mode);      //!< One of categories(), "Divers" if unknown

signals:
    void modeChosen(const QString& modeId);

private:
    QList<RBSMode> m_modes;
    QString m_currentId;
    QLineEdit *m_search;
    QListWidget *m_categories;
    QListWidget *m_list;

    void showModes();
};

#endif // INCLUDE_FEATURE_RBSMODEPOPUP_H_
