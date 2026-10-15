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

#ifndef INCLUDE_FEATURE_RBSFREQUENCYDIAL_H_
#define INCLUDE_FEATURE_RBSFREQUENCYDIAL_H_

#include <QWidget>

// Frequency display in the SDR++ style: flat digits grouped by three, leading zeros dimmed.
// The wheel over a digit, or a click on its upper or lower half, steps that digit up or down.
// A right click clears the digits to the right of the one clicked.
class RBSFrequencyDial : public QWidget
{
    Q_OBJECT
public:
    explicit RBSFrequencyDial(QWidget *parent = nullptr);

    void setValue(qint64 value);    //!< Does not emit changed()
    qint64 value() const { return m_value; }
    void setRange(qint64 minimum, qint64 maximum);
    bool isInteracting() const { return m_hoverDigit >= 0; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void changed(qint64 value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr int m_numDigits = 12;  //!< 000.000.000.000 Hz

    qint64 m_value;
    qint64 m_minimum;
    qint64 m_maximum;
    int m_hoverDigit;   //!< Power of ten under the mouse, -1 if none
    bool m_hoverUpper;

    int digitWidth() const;
    int separatorWidth() const;
    int digitAt(const QPoint& position) const;  //!< Power of ten at that position, -1 if none
    QRect digitRect(int power) const;
    void step(int power, int direction);
};

#endif // INCLUDE_FEATURE_RBSFREQUENCYDIAL_H_
