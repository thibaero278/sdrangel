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

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QTreeView>

#include "rbsdelegates.h"

RBSChipDelegate::RBSChipDelegate(const QColor& background, const QColor& text, bool bold, QObject *parent) :
    QStyledItemDelegate(parent),
    m_background(background),
    m_text(text),
    m_bold(bold)
{
}

void RBSChipDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Row background (selection, highlighted row) as usual, then the tag over it
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QString text = opt.text;
    opt.text.clear();
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    if (text.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QFont font = option.font;
    font.setBold(m_bold);
    painter->setFont(font);
    // Metrics of the painter's device, which can differ from the screen's: the text then fits exactly
    QFontMetrics metrics = painter->fontMetrics();
    int textWidth = metrics.horizontalAdvance(text) + 2;
    int maxTextWidth = option.rect.width() - 8 - 14;
    QString shown = textWidth <= maxTextWidth ? text : metrics.elidedText(text, Qt::ElideRight, maxTextWidth);
    int width = std::min(textWidth, maxTextWidth) + 14;
    QRectF chip(option.rect.left() + 4, option.rect.top() + 2, width, option.rect.height() - 4);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_background);
    painter->drawRoundedRect(chip, 3, 3);
    painter->setPen(m_text);
    painter->drawText(chip.adjusted(7, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, shown);
    painter->restore();
}

RBSCountryDelegate::RBSCountryDelegate(QTreeView *view) :
    QStyledItemDelegate(view),
    m_view(view)
{
}

void RBSCountryDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.parent().isValid())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // The country spans the whole row
    QRect row(0, option.rect.top(), m_view->viewport()->width(), option.rect.height());
    QRectF box = QRectF(row).adjusted(4.5, 2.5, -4.5, -1.5);
    bool hover = option.state & QStyle::State_MouseOver;
    bool expanded = m_view->isExpanded(index);
    painter->setPen(QColor(expanded ? "#3d8bff" : "#35353b"));
    painter->setBrush(QColor(hover ? "#2a2a2f" : "#212125"));
    painter->drawRoundedRect(box, 3, 3);

    QFont font = option.font;
    QFont bold = font;
    bold.setBold(true);
    QFontMetrics boldMetrics(bold);
    QRect content = box.toRect().adjusted(10, 0, -12, 0);
    int x = content.left();

    // Arrow, as the side menu sections
    painter->setFont(font);
    painter->setPen(QColor("#9696a0"));
    painter->drawText(QRect(x, content.top(), 14, content.height()), Qt::AlignCenter, expanded ? "▾" : "▸");
    x += 20;

    QString name = index.data(KeyRole).toString();
    painter->setFont(bold);
    painter->setPen(QColor("#ffffff"));
    painter->drawText(QRect(x, content.top(), boldMetrics.horizontalAdvance(name) + 2, content.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
    x += boldMetrics.horizontalAdvance(name) + 10;

    // Number of stations in a pill
    QString count = QString::number(index.data(StationsRole).toInt());
    int pillWidth = std::max(22, QFontMetrics(font).horizontalAdvance(count) + 12);
    QRectF pill(x, content.center().y() - 9, pillWidth, 18);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#35353b"));
    painter->drawRoundedRect(pill, 9, 9);
    painter->setFont(font);
    painter->setPen(QColor("#cfd6de"));
    painter->drawText(pill, Qt::AlignCenter, count);
    x += pillWidth + 8;

    // Notification dot while the country is new
    if (index.data(NewRole).toBool())
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#ef4444"));
        painter->drawEllipse(QPointF(x + 4, content.center().y()), 4.5, 4.5);
        painter->setPen(QColor("#ef4444"));
        painter->drawText(QRect(x + 13, content.top(), 80, content.height()), Qt::AlignLeft | Qt::AlignVCenter, "nouveau");
    }

    painter->setPen(QColor("#9696a0"));
    painter->drawText(content, Qt::AlignRight | Qt::AlignVCenter, index.data(SummaryRole).toString());
    painter->restore();
}

QSize RBSCountryDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);

    if (!index.parent().isValid()) {
        size.setHeight(34);
    }

    return size;
}
