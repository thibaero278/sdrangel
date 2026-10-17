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

#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"

#include "rbsspectrumtee.h"

RBSSpectrumTee::RBSSpectrumTee(GLSpectrum *rbsSpectrum) :
    m_rbsSpectrum(rbsSpectrum),
    m_original(nullptr),
    m_spectrumVis(nullptr),
    m_attaching(false),
    m_linesPerSecond(12),
    m_smoothing(0.0f),
    m_sumCount(0)
{
    m_lineTimer.start();
}

void RBSSpectrumTee::setLinesPerSecond(int linesPerSecond)
{
    QMutexLocker locker(&m_mutex);
    m_linesPerSecond = std::max(1, linesPerSecond);
}

void RBSSpectrumTee::setSmoothing(float smoothing)
{
    QMutexLocker locker(&m_mutex);
    m_smoothing = std::max(0.0f, std::min(0.95f, smoothing));
}

bool RBSSpectrumTee::getLastSpectrum(std::vector<float>& spectrum) const
{
    QMutexLocker locker(&m_mutex);
    spectrum = m_line;
    return !spectrum.empty();
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

    if (!m_rbsSpectrum || (fftSize <= 0)) {
        return;
    }

    if ((int) m_average.size() != fftSize)
    {
        m_average.assign(spectrum, spectrum + fftSize);
        m_sum.assign(fftSize, 0.0);
        m_sumCount = 0;
    }

    // Smoothing: exponential average, continuous. Without it: plain average of the spectra of the
    // line, so that a slow waterfall does not miss short signals
    if (m_smoothing > 0.0f)
    {
        for (int i = 0; i < fftSize; i++) {
            m_average[i] = m_smoothing * m_average[i] + (1.0f - m_smoothing) * spectrum[i];
        }
    }
    else
    {
        for (int i = 0; i < fftSize; i++) {
            m_sum[i] += spectrum[i];
        }

        m_sumCount++;
    }

    // One line per 1/linesPerSecond second
    if (m_lineTimer.elapsed() < 1000 / m_linesPerSecond) {
        return;
    }

    m_lineTimer.restart();

    if (m_smoothing > 0.0f)
    {
        m_line = m_average;
    }
    else
    {
        m_line.resize(fftSize);

        for (int i = 0; i < fftSize; i++)
        {
            m_line[i] = m_sumCount > 0 ? (float) (m_sum[i] / m_sumCount) : spectrum[i];
            m_sum[i] = 0.0;
        }

        m_sumCount = 0;
        m_average = m_line; // so that turning smoothing on starts from here
    }

    m_rbsSpectrum->newSpectrum(m_line.data(), fftSize);
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
