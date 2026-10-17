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

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "rbsmodepopup.h"

enum Roles {
    IdRole = Qt::UserRole,
    CountRole,
    TagRole
};

static const char* const m_styleSheet = R"(
RBSModePopup { background: #18181b; border: 1px solid #3d8bff; }
QLabel#rbsPopupTitle { color: #9696a0; font-size: 11px; padding: 6px 10px; background: #212125; border-bottom: 1px solid #35353b; }
QLineEdit#rbsPopupSearch { border: none; border-bottom: 1px solid #35353b; background: #212125; padding: 6px 10px; }
QListWidget { background: #18181b; border: none; outline: 0; }
QListWidget#rbsCategories { border-right: 1px solid #35353b; }
)";

// Row: name on the left, count on the right ("55" for a category, "12 fréq." for a mode),
// an optional tag in a frame before the count, as the MultiPSK Deck
class CountDelegate : public QStyledItemDelegate
{
public:
    CountDelegate(bool frequencies, QObject *parent) :
        QStyledItemDelegate(parent),
        m_frequencies(frequencies)
    {}

    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        bool selected = option.state & QStyle::State_Selected;
        bool hover = option.state & QStyle::State_MouseOver;
        bool empty = !(index.flags() & Qt::ItemIsEnabled);
        painter->fillRect(option.rect, selected ? QColor("#3d8bff") : hover ? QColor("#2a2a2f") : QColor("#18181b"));
        QRect r = option.rect.adjusted(12, 0, -12, 0);
        painter->setPen(QColor(selected ? "#ffffff" : empty ? "#55555d" : "#e3e3e6"));
        painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());

        int count = index.data(CountRole).toInt();
        QString countText = m_frequencies ? QString("%1 fréq.").arg(count) : QString::number(count);
        QFont small = option.font;
        small.setPointSizeF(option.font.pointSizeF() * 0.9);
        painter->setFont(small);
        painter->setPen(QColor(selected ? "#ffffff" : "#9696a0"));
        painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, countText);

        QString tag = index.data(TagRole).toString();

        if (!tag.isEmpty())
        {
            QFontMetrics metrics(small);
            int tagWidth = metrics.horizontalAdvance(tag) + 10;
            QRect tagRect(r.right() - metrics.horizontalAdvance(countText) - 12 - tagWidth, r.center().y() - 8, tagWidth, 16);
            painter->setPen(QColor(selected ? "#ffffff" : "#e0b040"));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(tagRect);
            painter->drawText(tagRect, Qt::AlignCenter, tag);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 32);
    }

private:
    bool m_frequencies;
};

const QStringList& RBSModePopup::categories()
{
    static const QStringList list = {
        "Amateur", "Aviation", "Maritime", "Météo", "Sondes & satellites", "Pro / militaire", "Voix numérique", "Divers"
    };
    return list;
}

QString RBSModePopup::categoryOf(const RBSMode& mode)
{
    return categories().contains(mode.m_group) ? mode.m_group : QString("Divers");
}

RBSModePopup::RBSModePopup(QWidget *parent) :
    QFrame(parent, Qt::Popup)
{
    setObjectName("RBSModePopup");
    setStyleSheet(m_styleSheet);
    setFixedSize(500, 440);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_search = new QLineEdit(this);
    m_search->setObjectName("rbsPopupSearch");
    m_search->setPlaceholderText("Chercher un mode…");
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    QHBoxLayout *lists = new QHBoxLayout();
    lists->setSpacing(0);

    QVBoxLayout *categoriesBox = new QVBoxLayout();
    categoriesBox->setSpacing(0);
    QLabel *categoriesTitle = new QLabel("CATÉGORIE", this);
    categoriesTitle->setObjectName("rbsPopupTitle");
    m_categories = new QListWidget(this);
    m_categories->setObjectName("rbsCategories");
    m_categories->setFixedWidth(210);
    m_categories->setMouseTracking(true);
    m_categories->setItemDelegate(new CountDelegate(false, m_categories));
    categoriesBox->addWidget(categoriesTitle);
    categoriesBox->addWidget(m_categories, 1);
    lists->addLayout(categoriesBox);

    QVBoxLayout *modesBox = new QVBoxLayout();
    modesBox->setSpacing(0);
    QLabel *modesTitle = new QLabel("MODES", this);
    modesTitle->setObjectName("rbsPopupTitle");
    m_list = new QListWidget(this);
    m_list->setMouseTracking(true);
    m_list->setItemDelegate(new CountDelegate(true, m_list));
    modesBox->addWidget(modesTitle);
    modesBox->addWidget(m_list, 1);
    lists->addLayout(modesBox, 1);
    layout->addLayout(lists, 1);

    connect(m_categories, &QListWidget::currentRowChanged, this, [this]() { showModes(); });
    connect(m_search, &QLineEdit::textChanged, this, [this]() { showModes(); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString id = item->data(IdRole).toString();
        hide();
        emit modeChosen(id);
    });
}

void RBSModePopup::showFor(QWidget *anchor, const QList<RBSMode>& modes, const QString& currentId)
{
    m_modes = modes;
    m_currentId = currentId;
    QString currentCategory;

    for (const auto& mode : modes)
    {
        if (mode.m_id == currentId) {
            currentCategory = categoryOf(mode);
        }
    }

    m_search->blockSignals(true);
    m_search->clear();
    m_search->blockSignals(false);
    m_categories->blockSignals(true);
    m_categories->clear();

    for (const auto& category : categories())
    {
        int count = 0;

        for (const auto& mode : modes) {
            count += categoryOf(mode) == category ? 1 : 0;
        }

        QListWidgetItem *item = new QListWidgetItem(category, m_categories);
        item->setData(CountRole, count);

        if (count == 0) {
            item->setToolTip("Pas encore de mode dans cette catégorie");
        }
        if (category == currentCategory) {
            m_categories->setCurrentItem(item);
        }
    }

    if (!m_categories->currentItem()) {
        m_categories->setCurrentRow(0);
    }

    m_categories->blockSignals(false);
    showModes();

    QPoint position = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    QRect screen = anchor->screen()->availableGeometry();
    position.setX(std::min(position.x(), screen.right() - width()));
    move(position);
    show();
    m_search->setFocus();
}

// The modes of the category, or those matching the search in all categories
void RBSModePopup::showModes()
{
    QString search = m_search->text().trimmed();
    QString category = m_categories->currentItem() ? m_categories->currentItem()->text() : QString();
    m_list->clear();

    for (const auto& mode : m_modes)
    {
        bool shown = search.isEmpty() ? (categoryOf(mode) == category) :
            (mode.m_name.contains(search, Qt::CaseInsensitive) || mode.m_demod.contains(search, Qt::CaseInsensitive)
             || mode.m_group.contains(search, Qt::CaseInsensitive));

        if (!shown) {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(mode.m_name, m_list);
        item->setData(IdRole, mode.m_id);
        item->setData(CountRole, mode.m_frequencies.size());
        item->setData(TagRole, mode.m_tag);
        item->setToolTip(mode.m_description);

        if (mode.m_id == m_currentId) {
            m_list->setCurrentItem(item);
        }
    }
}
