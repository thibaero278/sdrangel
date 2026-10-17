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

#include <QFormLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include "rbssection.h"

RBSSection::RBSSection(const QString& title, QWidget *parent) :
    QWidget(parent)
{
    setObjectName("rbsSection");

    m_header = new QToolButton(this);
    m_header->setObjectName("rbsSectionHeader");
    // As the MultiPSK Deck: small capitals, spaced
    QFont headerFont = m_header->font();
    headerFont.setBold(true);
    headerFont.setCapitalization(QFont::AllUppercase);
    headerFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    headerFont.setPointSizeF(headerFont.pointSizeF() * 0.9);
    m_header->setFont(headerFont);
    m_header->setText(title);
    m_header->setCheckable(true);
    m_header->setChecked(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::DownArrow);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setAutoRaise(true);

    m_body = new QWidget(this);
    m_body->setObjectName("rbsSectionBody");
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(8, 4, 8, 8);
    m_bodyLayout->setSpacing(4);
    m_form = new QFormLayout();
    m_form->setContentsMargins(0, 0, 0, 0);
    m_form->setHorizontalSpacing(8);
    m_form->setVerticalSpacing(4);
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_bodyLayout->addLayout(m_form);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_body);

    connect(m_header, &QToolButton::toggled, this, &RBSSection::setExpanded);
}

void RBSSection::setTitle(const QString& title)
{
    m_header->setText(title);
}

void RBSSection::addWidget(QWidget *widget)
{
    m_bodyLayout->addWidget(widget);
}

void RBSSection::setExpanded(bool expanded)
{
    m_header->blockSignals(true);
    m_header->setChecked(expanded);
    m_header->blockSignals(false);
    m_header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_body->setVisible(expanded);
}

bool RBSSection::isExpanded() const
{
    return !m_body->isHidden();
}
