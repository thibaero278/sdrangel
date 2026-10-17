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

#ifndef INCLUDE_FEATURE_RBSSPECTRUMTEE_H_
#define INCLUDE_FEATURE_RBSSPECTRUMTEE_H_

#include <vector>

#include <QElapsedTimer>
#include <QMutex>

#include "dsp/glspectruminterface.h"

class SpectrumVis;
class GLSpectrum;

// Sits between a device set's SpectrumVis and its display (the SDRangel spectrum, hidden while
// RBS is open), and also feeds the RBS spectrum. The RBS spectrum gets its own line rate (the
// waterfall speed, as the FFT rate of SDR++) and smoothing (exponential average of the spectra):
// the SDRangel display is left untouched.
class RBSSpectrumTee : public GLSpectrumInterface
{
public:
    explicit RBSSpectrumTee(GLSpectrum *rbsSpectrum);
    ~RBSSpectrumTee() override;

    //! Insert the tee between spectrumVis and its current display (original may be null)
    void attach(SpectrumVis *spectrumVis, GLSpectrum *original);
    //! Give the SpectrumVis its original display back
    void detach();
    bool isAttachedTo(const SpectrumVis *spectrumVis) const { return m_spectrumVis && (m_spectrumVis == spectrumVis); }

    void setLinesPerSecond(int linesPerSecond);   //!< Spectra (waterfall lines) sent to the RBS spectrum per second
    void setSmoothing(float smoothing);           //!< 0 (none) to 0.95: weight of the past in the average
    //! Last spectrum sent to the RBS spectrum (dB per bin, lowest frequency first). False if none yet
    bool getLastSpectrum(std::vector<float>& spectrum) const;

    void newSpectrum(const Real* spectrum, int fftSize) override;
    void setSpectrumVis(SpectrumVis *spectrumVis) override;

private:
    GLSpectrum *m_rbsSpectrum;
    GLSpectrum *m_original;
    SpectrumVis *m_spectrumVis;
    bool m_attaching;
    mutable QMutex m_mutex;

    int m_linesPerSecond;
    float m_smoothing;
    QElapsedTimer m_lineTimer;
    std::vector<float> m_average;  //!< Smoothed spectrum (exponential average)
    std::vector<double> m_sum;     //!< Sum of the spectra since the last line, without smoothing
    int m_sumCount;
    std::vector<float> m_line;     //!< Line sent last
};

#endif // INCLUDE_FEATURE_RBSSPECTRUMTEE_H_
