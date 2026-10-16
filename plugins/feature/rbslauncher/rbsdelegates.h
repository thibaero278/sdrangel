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

#ifndef INCLUDE_FEATURE_RBSDELEGATES_H_
#define INCLUDE_FEATURE_RBSDELEGATES_H_

#include <QColor>
#include <QStyledItemDelegate>

class QTreeView;

// Cell drawn as a coloured tag, so that call 1, call 2 and the message of an FT8 line read as
// three distinct things
class RBSChipDelegate : public QStyledItemDelegate
{
public:
    RBSChipDelegate(const QColor& background, const QColor& text, bool bold, QObject *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QColor m_background;
    QColor m_text;
    bool m_bold;
};

// Countries of the "group by country" view: each country is a box, as the side menu sections,
// with its number of stations, a summary on the right and a dot while it is new.
// Stations under it are drawn normally.
class RBSCountryDelegate : public QStyledItemDelegate
{
public:
    enum Roles {
        KeyRole = Qt::UserRole,     //!< Country name, without the count
        StationsRole,               //!< Number of stations
        SummaryRole,                //!< Right hand text
        NewRole                     //!< Not looked at yet
    };

    RBSCountryDelegate(QTreeView *view);
    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QTreeView *m_view;
};

#endif // INCLUDE_FEATURE_RBSDELEGATES_H_
