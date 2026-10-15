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

#ifndef INCLUDE_FEATURE_RBSSECTION_H_
#define INCLUDE_FEATURE_RBSSECTION_H_

#include <QWidget>

class QToolButton;
class QFormLayout;
class QVBoxLayout;

// Collapsible section, as in the SDR++ side menu: a title bar with an arrow, and a body
// that folds away when the title is clicked
class RBSSection : public QWidget
{
    Q_OBJECT
public:
    explicit RBSSection(const QString& title, QWidget *parent = nullptr);

    QWidget *body() { return m_body; }
    QFormLayout *form() { return m_form; }   //!< Label / control rows
    void addWidget(QWidget *widget);         //!< Full width row
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    QToolButton *m_header;
    QWidget *m_body;
    QVBoxLayout *m_bodyLayout;
    QFormLayout *m_form;
};

#endif // INCLUDE_FEATURE_RBSSECTION_H_
