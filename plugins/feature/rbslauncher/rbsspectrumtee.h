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

#include <QMutex>

#include "dsp/glspectruminterface.h"

class SpectrumVis;
class GLSpectrum;

// A SpectrumVis feeds a single display. This tee takes the place of the device set's own
// GLSpectrum and passes every spectrum on to it and to the RBS display, so both stay live.
//
// newSpectrum is called from the DSP thread; attach and detach from the main thread.
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

    void newSpectrum(const Real* spectrum, int fftSize) override;
    void setSpectrumVis(SpectrumVis *spectrumVis) override;

private:
    GLSpectrum *m_rbsSpectrum;
    GLSpectrum *m_original;
    SpectrumVis *m_spectrumVis;
    bool m_attaching;
    QMutex m_mutex;
};

#endif // INCLUDE_FEATURE_RBSSPECTRUMTEE_H_
