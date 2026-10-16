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

#include <cstdlib>

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "rbsbandpopup.h"

enum Roles {
    FrequencyRole = Qt::UserRole,
    NoteRole,
    StandardRole,
    CountRole
};

static const char* const m_styleSheet = R"(
RBSBandPopup { background: #14171c; border: 1px solid #3d8bff; }
QLabel#rbsPopupTitle { color: #7f8a98; font-size: 11px; padding: 6px 10px; background: #111418; border-bottom: 1px solid #252a31; }
QListWidget { background: #14171c; border: none; outline: 0; }
QListWidget#rbsBands { border-right: 1px solid #252a31; }
QWidget#rbsPopupBottom { background: #111418; border-top: 1px solid #252a31; }
)";

// Band: name on the left, number of frequencies on the right
class BandDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        bool selected = option.state & QStyle::State_Selected;
        bool hover = option.state & QStyle::State_MouseOver;
        painter->fillRect(option.rect, selected ? QColor("#3d8bff") : hover ? QColor("#1e232a") : QColor("#14171c"));
        QRect r = option.rect.adjusted(12, 0, -12, 0);
        painter->setPen(QColor(selected ? "#ffffff" : "#dfe4ea"));
        painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());
        painter->setPen(QColor(selected ? "#ffffff" : "#7f8a98"));
        painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, QString::number(index.data(CountRole).toInt()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 32);
    }
};

// Frequency: "14,074 MHz  STANDARD  USB" over the note, as in the reference design
class FrequencyDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void setDemod(const QString& demod) { m_demod = demod; }

    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        bool selected = option.state & QStyle::State_Selected;
        bool hover = option.state & QStyle::State_MouseOver;
        painter->fillRect(option.rect, selected ? QColor("#262c35") : hover ? QColor("#1e232a") : QColor("#14171c"));
        painter->setPen(QColor("#1c2026"));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

        QRect r = option.rect.adjusted(14, 6, -10, -6);
        QFont bold = option.font;
        bold.setBold(true);
        bold.setPointSizeF(option.font.pointSizeF() * 1.1);
        QFont small = option.font;
        small.setPointSizeF(option.font.pointSizeF() * 0.85);

        QString frequency = index.data(Qt::DisplayRole).toString();
        painter->setFont(bold);
        painter->setPen(QColor("#ffffff"));
        int top = r.top() + QFontMetrics(bold).ascent();
        painter->drawText(r.left(), top, frequency);
        int x = r.left() + QFontMetrics(bold).horizontalAdvance(frequency) + 10;

        painter->setFont(small);

        if (index.data(StandardRole).toBool())
        {
            painter->setPen(QColor("#3ecf7a"));
            painter->drawText(x, top, "STANDARD");
            x += QFontMetrics(small).horizontalAdvance("STANDARD") + 8;
        }

        painter->setPen(QColor("#7f8a98"));
        painter->drawText(x, top, m_demod);

        painter->setPen(QColor("#7f8a98"));
        painter->drawText(r, Qt::AlignLeft | Qt::AlignBottom, index.data(NoteRole).toString());
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 50);
    }

private:
    QString m_demod;
};

RBSBandPopup::RBSBandPopup(QWidget *parent) :
    QFrame(parent, Qt::Popup),
    m_frequency(0)
{
    setObjectName("RBSBandPopup");
    setStyleSheet(m_styleSheet);
    setFixedSize(560, 460);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    QHBoxLayout *lists = new QHBoxLayout();
    lists->setSpacing(0);

    QVBoxLayout *bandsBox = new QVBoxLayout();
    bandsBox->setSpacing(0);
    m_bandsTitle = new QLabel(this);
    m_bandsTitle->setObjectName("rbsPopupTitle");
    m_bands = new QListWidget(this);
    m_bands->setObjectName("rbsBands");
    m_bands->setFixedWidth(210);
    m_bands->setItemDelegate(new BandDelegate(m_bands));
    m_bands->setMouseTracking(true);
    bandsBox->addWidget(m_bandsTitle);
    bandsBox->addWidget(m_bands, 1);
    lists->addLayout(bandsBox);

    QVBoxLayout *frequenciesBox = new QVBoxLayout();
    frequenciesBox->setSpacing(0);
    QLabel *frequenciesTitle = new QLabel("FRÉQUENCES", this);
    frequenciesTitle->setObjectName("rbsPopupTitle");
    m_frequencies = new QListWidget(this);
    m_frequencies->setMouseTracking(true);
    m_frequencies->setItemDelegate(new FrequencyDelegate(m_frequencies));
    frequenciesBox->addWidget(frequenciesTitle);
    frequenciesBox->addWidget(m_frequencies, 1);
    lists->addLayout(frequenciesBox, 1);
    layout->addLayout(lists, 1);

    // Bottom: frequency in kHz, then steps
    QWidget *bottom = new QWidget(this);
    bottom->setObjectName("rbsPopupBottom");
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(8, 8, 8, 8);
    bottomLayout->setSpacing(4);
    m_entry = new QLineEdit(bottom);
    m_entry->setFixedWidth(120);
    m_entry->setToolTip("Fréquence en kHz, puis Entrée");
    QToolButton *go = new QToolButton(bottom);
    go->setText("Aller");
    go->setStyleSheet("QToolButton { background: #3d8bff; border-color: #3d8bff; color: #ffffff; padding: 3px 14px; }");
    bottomLayout->addWidget(m_entry);
    bottomLayout->addWidget(go);
    bottomLayout->addStretch(1);

    const int steps[] = {-10000, -1000, 1000, 10000};

    for (int step : steps)
    {
        QToolButton *button = new QToolButton(bottom);
        button->setText(QString("%1%2k").arg(step < 0 ? "−" : "+").arg(std::abs(step) / 1000));
        connect(button, &QToolButton::clicked, this, [this, step]() { choose(m_frequency + step, false); });
        bottomLayout->addWidget(button);
    }

    layout->addWidget(bottom);

    auto enter = [this]() {
        bool ok;
        double kHz = m_entry->text().trimmed().replace(',', '.').remove(' ').toDouble(&ok);

        if (ok && (kHz > 0)) {
            choose(qRound64(kHz * 1000.0), true);
        }
    };
    connect(go, &QToolButton::clicked, this, enter);
    connect(m_entry, &QLineEdit::returnPressed, this, enter);
    connect(m_bands, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            showBand(m_bands->item(row)->text());
        }
    });
    connect(m_frequencies, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        choose(item->data(FrequencyRole).toLongLong(), true);
    });
}

QString RBSBandPopup::formatMHz(qint64 frequency)
{
    QString text = QString::number(frequency / 1e6, 'f', 3);

    while (text.endsWith('0')) {
        text.chop(1);
    }
    if (text.endsWith('.')) {
        text.chop(1);
    }

    // French: decimal comma, thin space between thousands of MHz
    QString integer = text.section('.', 0, 0);
    QString decimals = text.section('.', 1, 1);

    if (integer.size() > 3) {
        integer.insert(integer.size() - 3, QChar(0x202F));
    }

    return (decimals.isEmpty() ? integer : integer + "," + decimals) + " MHz";
}

void RBSBandPopup::showFor(QWidget *anchor, const RBSMode& mode, qint64 frequency)
{
    m_mode = mode;
    m_frequency = frequency;
    m_entry->setText(QString::number(frequency / 1000.0, 'f', frequency % 1000 ? 3 : 0));
    m_bandsTitle->setText(QString("BANDES · %1").arg(mode.m_name.toUpper()));
    static_cast<FrequencyDelegate*>(m_frequencies->itemDelegate())->setDemod(mode.m_demod);

    // Bands in the order of the mode sheet
    QStringList bands;

    for (const auto& known : mode.m_frequencies)
    {
        if (!bands.contains(known.m_band)) {
            bands.append(known.m_band);
        }
    }

    QString current = mode.bandOf(frequency);
    m_bands->blockSignals(true);
    m_bands->clear();

    for (const auto& band : bands)
    {
        int count = 0;

        for (const auto& known : mode.m_frequencies) {
            count += known.m_band == band ? 1 : 0;
        }

        QListWidgetItem *item = new QListWidgetItem(band, m_bands);
        item->setData(CountRole, count);

        if (band == current) {
            m_bands->setCurrentItem(item);
        }
    }

    if (!m_bands->currentItem() && (m_bands->count() > 0)) {
        m_bands->setCurrentRow(0);
    }

    m_bands->blockSignals(false);
    showBand(m_bands->currentItem() ? m_bands->currentItem()->text() : QString());

    // Under the anchor, kept on the screen
    QPoint position = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    QRect screen = anchor->screen()->availableGeometry();
    position.setX(std::min(position.x(), screen.right() - width()));
    move(position);
    show();
    m_entry->setFocus();
    m_entry->selectAll();
}

void RBSBandPopup::showBand(const QString& band)
{
    m_frequencies->clear();

    for (const auto& known : m_mode.m_frequencies)
    {
        if (known.m_band != band) {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(formatMHz(known.m_frequency), m_frequencies);
        item->setData(FrequencyRole, known.m_frequency);
        item->setData(NoteRole, known.m_note);
        item->setData(StandardRole, known.m_standard);

        if (known.m_frequency == m_frequency) {
            m_frequencies->setCurrentItem(item);
        }
    }
}

void RBSBandPopup::choose(qint64 frequency, bool close)
{
    if (frequency <= 0) {
        return;
    }

    m_frequency = frequency;
    m_entry->setText(QString::number(frequency / 1000.0, 'f', frequency % 1000 ? 3 : 0));
    emit frequencyChosen(frequency);

    if (close) {
        hide();
    }
}
