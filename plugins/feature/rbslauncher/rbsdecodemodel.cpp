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

#include <QBrush>
#include <QColor>
#include <QFont>

#include "rbsdecodemodel.h"

RBSDecodeModel::RBSDecodeModel(QObject *parent) :
    QAbstractTableModel(parent),
    m_boldColumn(-1)
{
}

void RBSDecodeModel::setColumns(const QStringList& columns, int boldColumn)
{
    beginResetModel();
    m_columns = columns;
    m_boldColumn = boldColumn;
    m_rows.clear();
    endResetModel();
}

void RBSDecodeModel::addRow(const QStringList& cells, Kind kind)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_rows.prepend(Row{QString(), cells, kind});
    endInsertRows();

    if (m_rows.size() > m_maxRows)
    {
        beginRemoveRows(QModelIndex(), m_maxRows, m_rows.size() - 1);
        m_rows.erase(m_rows.begin() + m_maxRows, m_rows.end());
        endRemoveRows();
    }
}

void RBSDecodeModel::upsertRow(const QString& key, const QStringList& cells, Kind kind)
{
    for (int i = 0; i < m_rows.size(); i++)
    {
        if (m_rows[i].m_key == key)
        {
            // Nothing new (aircraft refreshed every second): no move, no redraw, no refiltering
            if ((m_rows[i].m_cells == cells) && (m_rows[i].m_kind == kind)) {
                return;
            }

            // Move the updated row to the top so the most recently heard is first
            if (i > 0)
            {
                beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0);
                m_rows.move(i, 0);
                endMoveRows();
            }

            m_rows[0].m_cells = cells;
            m_rows[0].m_kind = kind;
            emit dataChanged(index(0, 0), index(0, m_columns.size() - 1));
            return;
        }
    }

    addRow(cells, kind);
    m_rows[0].m_key = key;
}

void RBSDecodeModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

QString RBSDecodeModel::toCSV() const
{
    QStringList lines;
    lines.append(m_columns.join(';'));

    for (int i = m_rows.size() - 1; i >= 0; i--) {
        lines.append(m_rows[i].m_cells.join(';'));
    }

    return lines.join('\n') + '\n';
}

int RBSDecodeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int RBSDecodeModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

QVariant RBSDecodeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (index.row() >= m_rows.size())) {
        return QVariant();
    }

    const Row& row = m_rows[index.row()];

    switch (role)
    {
    case Qt::DisplayRole:
        return index.column() < row.m_cells.size() ? row.m_cells[index.column()] : QString();
    case Qt::ForegroundRole:
        if (row.m_kind == Highlight) {
            return QBrush(QColor(62, 207, 122));
        }
        break;
    case Qt::BackgroundRole:
        if (row.m_kind == Mine) {
            return QBrush(QColor(19, 33, 58));
        }
        break;
    case Qt::FontRole:
        if (index.column() == m_boldColumn)
        {
            static const QFont bold = []() { QFont font; font.setBold(true); return font; }();
            return bold;
        }
        break;
    case KindRole:
        return (int) row.m_kind;
    default:
        break;
    }

    return QVariant();
}

QVariant RBSDecodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole) && (section < m_columns.size())) {
        return m_columns[section];
    }

    return QVariant();
}

RBSDecodeFilter::RBSDecodeFilter(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_highlightedOnly(false)
{
}

void RBSDecodeFilter::setSearch(const QString& text)
{
    m_search = text.trimmed();
    invalidateFilter();
}

void RBSDecodeFilter::setHighlightedOnly(bool highlightedOnly)
{
    m_highlightedOnly = highlightedOnly;
    invalidateFilter();
}

void RBSDecodeFilter::setRequirement(const QString& name, int column, const QString& value, bool exact)
{
    m_requirements.insert(name, Requirement{column, value, exact});
    invalidateFilter();
}

void RBSDecodeFilter::removeRequirement(const QString& name)
{
    if (m_requirements.remove(name) > 0) {
        invalidateFilter();
    }
}

void RBSDecodeFilter::clearRequirements()
{
    if (!m_requirements.isEmpty())
    {
        m_requirements.clear();
        invalidateFilter();
    }
}

bool RBSDecodeFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel *model = sourceModel();

    for (const auto& requirement : m_requirements)
    {
        QString cell = model->data(model->index(sourceRow, requirement.m_column, sourceParent)).toString();

        if (requirement.m_value.isEmpty() ? cell.isEmpty() :
            requirement.m_exact ? (cell.compare(requirement.m_value, Qt::CaseInsensitive) != 0) :
            !cell.startsWith(requirement.m_value, Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (m_highlightedOnly)
    {
        int kind = model->data(model->index(sourceRow, 0, sourceParent), RBSDecodeModel::KindRole).toInt();

        if (kind == RBSDecodeModel::Normal) {
            return false;
        }
    }

    if (m_search.isEmpty()) {
        return true;
    }

    for (int c = 0; c < model->columnCount(sourceParent); c++)
    {
        if (model->data(model->index(sourceRow, c, sourceParent)).toString().contains(m_search, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}
