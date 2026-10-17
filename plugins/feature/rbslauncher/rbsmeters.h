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

#ifndef INCLUDE_FEATURE_RBSMETERS_H_
#define INCLUDE_FEATURE_RBSMETERS_H_

#include <QWidget>

// Top bar meters: S/N and level as bars (green to red), quality as five steps, with their values
class RBSMeters : public QWidget
{
    Q_OBJECT
public:
    explicit RBSMeters(QWidget *parent = nullptr);

    void setSNR(double snrDB, bool valid);       //!< Signal in the decoder passband over the noise floor
    void setLevel(double levelDB, bool valid);   //!< Channel power
    void setQuality(int steps, const QString& text);  //!< 0 to 5

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_snr;
    bool m_snrValid;
    double m_level;
    bool m_levelValid;
    int m_quality;
    QString m_qualityText;
};

#endif // INCLUDE_FEATURE_RBSMETERS_H_
