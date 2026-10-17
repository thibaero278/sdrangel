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

#include <algorithm>

#include <QLinearGradient>
#include <QPainter>

#include "rbsmeters.h"

static constexpr int m_rowHeight = 16;
static constexpr int m_labelWidth = 62;
static constexpr int m_barWidth = 96;
static constexpr int m_valueWidth = 64;

RBSMeters::RBSMeters(QWidget *parent) :
    QWidget(parent),
    m_snr(0.0),
    m_snrValid(false),
    m_level(-130.0),
    m_levelValid(false),
    m_quality(0)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip("S/N : signal dans la bande du décodeur au-dessus du bruit\n"
        "Niveau : puissance du canal\n"
        "Qualité : d'après le S/N");
}

QSize RBSMeters::sizeHint() const
{
    return QSize(m_labelWidth + m_barWidth + m_valueWidth + 16, 3 * m_rowHeight + 2);
}

void RBSMeters::setSNR(double snrDB, bool valid)
{
    m_snr = snrDB;
    m_snrValid = valid;
    update();
}

void RBSMeters::setLevel(double levelDB, bool valid)
{
    m_level = levelDB;
    m_levelValid = valid;
    update();
}

void RBSMeters::setQuality(int steps, const QString& text)
{
    m_quality = std::max(0, std::min(5, steps));
    m_qualityText = text;
    update();
}

void RBSMeters::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter painter(this);
    QFont label = font();
    label.setPointSizeF(font().pointSizeF() * 0.82);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 106);
    QFont value = font();
    value.setPointSizeF(font().pointSizeF() * 0.92);
    const QColor muted("#9696a0");
    const QColor text("#e3e3e6");
    const QColor panel("#212125");
    const QColor line("#35353b");
    const QColor ok("#4dbf7a");

    auto drawBar = [&](int row, double fraction) {
        QRect bar(m_labelWidth, row * m_rowHeight + (m_rowHeight - 8) / 2, m_barWidth, 8);
        painter.setPen(line);
        painter.setBrush(panel);
        painter.drawRect(bar.adjusted(0, 0, -1, -1));
        int width = (int) ((bar.width() - 2) * std::max(0.0, std::min(1.0, fraction)));

        if (width > 0)
        {
            QLinearGradient gradient(bar.left(), 0, bar.right(), 0);
            gradient.setColorAt(0.0, ok);
            gradient.setColorAt(0.75, QColor("#e0b040"));
            gradient.setColorAt(1.0, QColor("#e05a50"));
            painter.fillRect(QRect(bar.left() + 1, bar.top() + 1, width, bar.height() - 2), gradient);
        }
    };
    auto drawLabels = [&](int row, const QString& name, const QString& shown) {
        QRect rowRect(0, row * m_rowHeight, width(), m_rowHeight);
        painter.setFont(label);
        painter.setPen(muted);
        painter.drawText(rowRect.adjusted(0, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, name);
        painter.setFont(value);
        painter.setPen(text);
        painter.drawText(QRect(m_labelWidth + m_barWidth + 10, row * m_rowHeight, m_valueWidth, m_rowHeight),
            Qt::AlignLeft | Qt::AlignVCenter, shown);
    };

    // S/N: 0 to 40 dB, level: -130 to -30 dB
    drawBar(0, m_snrValid ? m_snr / 40.0 : 0.0);
    drawLabels(0, "S/N", m_snrValid ? QString("%1 dB").arg(qRound(m_snr)) : QString("—"));
    drawBar(1, m_levelValid ? (m_level + 130.0) / 100.0 : 0.0);
    drawLabels(1, "Niveau", m_levelValid ? QString("%1 dB").arg(m_level, 0, 'f', 1) : QString("—"));

    // Quality: five steps of growing height
    for (int i = 0; i < 5; i++)
    {
        int height = 4 + 2 * i;
        QRect step(m_labelWidth + i * 9, 2 * m_rowHeight + (m_rowHeight + 11) / 2 - height, 7, height);
        bool on = i < m_quality;
        painter.setPen(on ? ok : line);
        painter.setBrush(on ? ok : panel);
        painter.drawRect(step.adjusted(0, 0, -1, -1));
    }

    drawLabels(2, "Qualité", m_qualityText.isEmpty() ? QString("—") : m_qualityText);
}
