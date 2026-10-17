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

    void setColumns(const QStringList& columns, int boldColumn = -1);
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
    int m_boldColumn;   //!< The most important column (the calling station), -1 if none
    QList<Row> m_rows;  //!< m_rows[0] is the newest
};

// Text search over all columns, an optional "highlighted rows only" filter, and column
// requirements of the decoder panel ("CQ only": call 1 starts with CQ, "with position"...)
class RBSDecodeFilter : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit RBSDecodeFilter(QObject *parent = nullptr);
    void setSearch(const QString& text);
    void setHighlightedOnly(bool highlightedOnly);
    //! The cell of column must start with value (equal it if exact; not be empty if value is empty).
    //! Replaces the requirement of the same name
    void setRequirement(const QString& name, int column, const QString& value, bool exact = false);
    void removeRequirement(const QString& name);
    void clearRequirements();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    struct Requirement
    {
        int m_column;
        QString m_value;
        bool m_exact;
    };

    QString m_search;
    bool m_highlightedOnly;
    QHash<QString, Requirement> m_requirements;
};

#endif // INCLUDE_FEATURE_RBSDECODEMODEL_H_
