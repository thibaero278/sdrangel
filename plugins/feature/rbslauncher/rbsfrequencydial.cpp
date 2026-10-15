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

#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QWheelEvent>

#include "rbsfrequencydial.h"

static const QColor m_digitColor(223, 228, 234);
static const QColor m_leadingZeroColor(58, 65, 75);
static const QColor m_hoverColor(61, 139, 255, 70);

RBSFrequencyDial::RBSFrequencyDial(QWidget *parent) :
    QWidget(parent),
    m_value(0),
    m_minimum(0),
    m_maximum(999999999999LL),
    m_hoverDigit(-1),
    m_hoverUpper(true)
{
    QFont f = font();
    f.setPointSize(24);
    setFont(f);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip("Molette ou clic en haut / en bas d'un chiffre pour le changer. Clic droit : remet à zéro les chiffres à droite.");
}

void RBSFrequencyDial::setValue(qint64 value)
{
    value = std::max(m_minimum, std::min(m_maximum, value));

    if (value != m_value)
    {
        m_value = value;
        update();
    }
}

void RBSFrequencyDial::setRange(qint64 minimum, qint64 maximum)
{
    m_minimum = minimum;
    m_maximum = maximum;
    setValue(m_value);
}

int RBSFrequencyDial::digitWidth() const
{
    return fontMetrics().horizontalAdvance('0') + 1;
}

int RBSFrequencyDial::separatorWidth() const
{
    return fontMetrics().horizontalAdvance('.');
}

QSize RBSFrequencyDial::sizeHint() const
{
    int groups = (m_numDigits - 1) / 3;
    return QSize(m_numDigits * digitWidth() + groups * separatorWidth() + 4, fontMetrics().height() + 4);
}

// Digits are drawn left to right, from the power m_numDigits - 1 down to 0
QRect RBSFrequencyDial::digitRect(int power) const
{
    int index = m_numDigits - 1 - power;          // 0 for the leftmost digit
    int separatorsBefore = index / 3;
    int x = 2 + index * digitWidth() + separatorsBefore * separatorWidth();
    return QRect(x, 2, digitWidth(), height() - 4);
}

int RBSFrequencyDial::digitAt(const QPoint& position) const
{
    for (int power = 0; power < m_numDigits; power++)
    {
        if (digitRect(power).contains(position)) {
            return power;
        }
    }

    return -1;
}

void RBSFrequencyDial::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter painter(this);
    painter.setFont(font());
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QString digits = QString::number(m_value).rightJustified(m_numDigits, '0');
    int firstSignificant = digits.indexOf(QRegularExpression("[1-9]"));

    if (firstSignificant < 0) {
        firstSignificant = m_numDigits - 1;
    }

    for (int index = 0; index < m_numDigits; index++)
    {
        int power = m_numDigits - 1 - index;
        QRect rect = digitRect(power);

        if (power == m_hoverDigit)
        {
            QRect half = m_hoverUpper ? QRect(rect.x(), rect.y(), rect.width(), rect.height() / 2)
                                      : QRect(rect.x(), rect.y() + rect.height() / 2, rect.width(), rect.height() - rect.height() / 2);
            painter.fillRect(half, m_hoverColor);
        }

        painter.setPen(index < firstSignificant ? m_leadingZeroColor : m_digitColor);
        painter.drawText(rect, Qt::AlignCenter, digits.mid(index, 1));

        if ((power > 0) && (power % 3 == 0))
        {
            QRect separator(rect.right() + 1, rect.y(), separatorWidth(), rect.height());
            painter.drawText(separator, Qt::AlignCenter, ".");
        }
    }
}

void RBSFrequencyDial::step(int power, int direction)
{
    qint64 increment = 1;

    for (int i = 0; i < power; i++) {
        increment *= 10;
    }

    qint64 value = std::max(m_minimum, std::min(m_maximum, m_value + direction * increment));

    if (value != m_value)
    {
        m_value = value;
        update();
        emit changed(m_value);
    }
}

void RBSFrequencyDial::wheelEvent(QWheelEvent *event)
{
    int power = digitAt(event->position().toPoint());

    if (power >= 0) {
        step(power, event->angleDelta().y() > 0 ? 1 : -1);
    }

    event->accept();
}

void RBSFrequencyDial::mousePressEvent(QMouseEvent *event)
{
    int power = digitAt(event->position().toPoint());

    if (power < 0) {
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        bool upper = event->position().y() < digitRect(power).center().y();
        step(power, upper ? 1 : -1);
    }
    else if (event->button() == Qt::RightButton)
    {
        // Clear the digits to the right of this one
        qint64 unit = 1;

        for (int i = 0; i < power; i++) {
            unit *= 10;
        }

        qint64 value = (m_value / unit) * unit;

        if (value != m_value)
        {
            m_value = std::max(m_minimum, value);
            update();
            emit changed(m_value);
        }
    }
}

void RBSFrequencyDial::mouseMoveEvent(QMouseEvent *event)
{
    int power = digitAt(event->position().toPoint());
    bool upper = (power >= 0) && (event->position().y() < digitRect(power).center().y());

    if ((power != m_hoverDigit) || (upper != m_hoverUpper))
    {
        m_hoverDigit = power;
        m_hoverUpper = upper;
        update();
    }
}

void RBSFrequencyDial::leaveEvent(QEvent *event)
{
    (void) event;
    m_hoverDigit = -1;
    update();
}
