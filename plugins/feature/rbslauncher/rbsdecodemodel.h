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

#ifndef INCLUDE_FEATURE_RBSDECODEMODEL_H_
#define INCLUDE_FEATURE_RBSDECODEMODEL_H_

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSortFilterProxyModel>
#include <QStringList>

// Rows of the decode table. The columns depend on the mode (FT8 messages, APRS stations,
// aircraft). Newest rows are on top. Rows with a key are updated in place (one row per
// station or aircraft), rows without are appended (one row per FT8 message).
class RBSDecodeModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Kind {
        Normal,
        Highlight,   //!< CQ, new station...: drawn in green
        Mine         //!< Message for my callsign: highlighted row
    };

    enum Roles {
        KindRole = Qt::UserRole
    };

    explicit RBSDecodeModel(QObject *parent = nullptr);

    void setColumns(const QStringList& columns);
    void addRow(const QStringList& cells, Kind kind = Normal);
    void upsertRow(const QString& key, const QStringList& cells, Kind kind = Normal);
    void clear();
    QString toCSV() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    static constexpr int m_maxRows = 2000;

private:
    struct Row {
        QString m_key;
        QStringList m_cells;
        Kind m_kind;
    };

    QStringList m_columns;
    QList<Row> m_rows;  //!< m_rows[0] is the newest
};

// Text search over all columns, plus an optional "highlighted rows only" (CQ only) filter
class RBSDecodeFilter : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit RBSDecodeFilter(QObject *parent = nullptr);
    void setSearch(const QString& text);
    void setHighlightedOnly(bool highlightedOnly);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_search;
    bool m_highlightedOnly;
};

#endif // INCLUDE_FEATURE_RBSDECODEMODEL_H_
