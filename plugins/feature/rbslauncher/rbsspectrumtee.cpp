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

#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"

#include "rbsspectrumtee.h"

RBSSpectrumTee::RBSSpectrumTee(GLSpectrum *rbsSpectrum) :
    m_rbsSpectrum(rbsSpectrum),
    m_original(nullptr),
    m_spectrumVis(nullptr),
    m_attaching(false)
{
}

RBSSpectrumTee::~RBSSpectrumTee()
{
    detach();
}

void RBSSpectrumTee::attach(SpectrumVis *spectrumVis, GLSpectrum *original)
{
    detach();

    {
        QMutexLocker locker(&m_mutex);
        m_original = original;
    }

    // setGLSpectrum calls back setSpectrumVis(nullptr) on the original display then
    // setSpectrumVis(spectrumVis) on us: the flag keeps the original attached meanwhile
    m_attaching = true;
    spectrumVis->setGLSpectrum(this);
    m_attaching = false;
}

void RBSSpectrumTee::detach()
{
    SpectrumVis *spectrumVis;
    GLSpectrum *original;

    {
        QMutexLocker locker(&m_mutex);
        spectrumVis = m_spectrumVis;
        original = m_original;
    }

    if (spectrumVis) {
        spectrumVis->setGLSpectrum(original); // calls setSpectrumVis(nullptr) on us
    }

    QMutexLocker locker(&m_mutex);
    m_spectrumVis = nullptr;
    m_original = nullptr;
}

void RBSSpectrumTee::newSpectrum(const Real* spectrum, int fftSize)
{
    QMutexLocker locker(&m_mutex);

    if (m_original) {
        m_original->newSpectrum(spectrum, fftSize);
    }
    if (m_rbsSpectrum) {
        m_rbsSpectrum->newSpectrum(spectrum, fftSize);
    }
}

void RBSSpectrumTee::setSpectrumVis(SpectrumVis *spectrumVis)
{
    if (m_attaching && !spectrumVis) {
        return; // previous display being released while we take its place: see attach()
    }

    QMutexLocker locker(&m_mutex);
    m_spectrumVis = spectrumVis;

    // The original display talks back to the SpectrumVis (zoom, settings), so it must keep
    // knowing it. When the SpectrumVis goes (device set removed), forget the original too:
    // it is deleted right after
    if (m_original) {
        m_original->setSpectrumVis(spectrumVis);
    }

    if (!spectrumVis) {
        m_original = nullptr;
    }
}
