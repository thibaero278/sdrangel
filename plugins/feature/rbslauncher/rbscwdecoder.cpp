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
#include <cmath>
#include <map>

#include "rbscwdecoder.h"

static constexpr double m_pi = 3.14159265358979323846;
static constexpr int m_fftHop = 256;               //!< 32 ms at 8 kHz
static constexpr int m_levelWindowMs = 2000;       //!< Noise and carrier levels over 2 s
static constexpr int m_levelUpdateMs = 20;
static constexpr float m_presenceRatio = 6.3f;     //!< 8 dB: averaged FFT peak over median
static constexpr double m_presenceHoldS = 1.5;     //!< Signal kept that long after the tone was last seen
static constexpr double m_hysteresis = 1.15;       //!< About +-1.2 dB around the threshold
static constexpr float m_minLevelSpreadDB = 6.0f;  //!< Carrier must be that much over the noise
static constexpr int m_markHistorySize = 50;
static constexpr double m_ditMinMs = 20.0;         //!< 60 WPM
static constexpr double m_ditMaxMs = 240.0;        //!< 5 WPM

// In place radix 2 FFT
static void fft(std::vector<std::complex<double>>& x)
{
    const int n = (int) x.size();

    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;

        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }

        j ^= bit;

        if (i < j) {
            std::swap(x[i], x[j]);
        }
    }

    for (int length = 2; length <= n; length <<= 1)
    {
        double angle = -2.0 * m_pi / length;
        std::complex<double> step(std::cos(angle), std::sin(angle));

        for (int i = 0; i < n; i += length)
        {
            std::complex<double> w(1.0, 0.0);

            for (int k = 0; k < length / 2; k++)
            {
                std::complex<double> u = x[i + k];
                std::complex<double> v = x[i + k + length / 2] * w;
                x[i + k] = u + v;
                x[i + k + length / 2] = u - v;
                w *= step;
            }
        }
    }
}

RBSCWDecoder::RBSCWDecoder(int sampleRate) :
    m_sampleRate(0)
{
    setSampleRate(sampleRate);
}

void RBSCWDecoder::setSampleRate(int sampleRate)
{
    if ((sampleRate == m_sampleRate) || (sampleRate <= 0)) {
        return;
    }

    m_sampleRate = sampleRate;
    m_decimation = std::max(1, (int) std::lround(sampleRate / 8000.0));
    m_rate = (double) sampleRate / m_decimation;

    // Anti alias low pass for the decimation: windowed sinc, cut at 45 % of the new rate
    m_aaTaps.clear();

    if (m_decimation > 1)
    {
        int length = 8 * m_decimation + 1;
        double cutoff = 0.45 * m_rate / sampleRate;
        double sum = 0.0;

        for (int i = 0; i < length; i++)
        {
            double t = i - (length - 1) / 2.0;
            double sinc = t == 0.0 ? 2.0 * cutoff : std::sin(2.0 * m_pi * cutoff * t) / (m_pi * t);
            double hamming = 0.54 - 0.46 * std::cos(2.0 * m_pi * i / (length - 1));
            m_aaTaps.push_back((float) (sinc * hamming));
            sum += sinc * hamming;
        }

        for (auto& tap : m_aaTaps) {
            tap = (float) (tap / sum);
        }
    }

    m_window.resize(m_fftSize);

    for (int i = 0; i < m_fftSize; i++) {
        m_window[i] = (float) (0.5 - 0.5 * std::cos(2.0 * m_pi * i / (m_fftSize - 1)));
    }

    m_envelopeDecimation = std::max(1, (int) std::lround(m_rate / 1000.0));
    m_envelopeAlpha = 1.0 - std::exp(-1.0 / (0.005 * m_rate));
    reset();
}

void RBSCWDecoder::setSettings(const Settings& settings)
{
    bool filterChanged = settings.m_filterWidthHz != m_settings.m_filterWidthHz;
    m_settings = settings;

    if (filterChanged)
    {
        m_filterHz = 0.0f;
        updateFilter();
    }
    if (settings.m_toneLocked) {
        m_ncoHz = settings.m_lockedToneHz;
    }
    if (!settings.m_autoSpeed) {
        m_ditMs = 1200.0 / std::max(5.0f, std::min(60.0f, settings.m_manualWPM));
    }
}

// Automatic width: wide enough for the marks to rise in a fifth of a dit, no wider (noise)
void RBSCWDecoder::updateFilter()
{
    float width = m_settings.m_filterWidthHz > 0.0f ?
        m_settings.m_filterWidthHz :
        (float) std::max(100.0, std::min(400.0, 5000.0 / m_ditMs));

    if (std::abs(width - m_filterHz) > 0.15f * m_filterHz)
    {
        m_filterHz = width;
        design();
    }
}

// Narrow low pass after the NCO: 4th order Butterworth at half the filter width, as two
// second order sections (RBJ cookbook)
void RBSCWDecoder::design()
{
    static const double q[2] = {0.54119610, 1.30656296};
    double cutoff = std::max(10.0, std::min(1000.0, m_filterHz / 2.0));
    double w0 = 2.0 * m_pi * cutoff / m_rate;

    for (int s = 0; s < 2; s++)
    {
        double alpha = std::sin(w0) / (2.0 * q[s]);
        double cosw0 = std::cos(w0);
        double a0 = 1.0 + alpha;
        m_lowPass[s].b0 = ((1.0 - cosw0) / 2.0) / a0;
        m_lowPass[s].b1 = (1.0 - cosw0) / a0;
        m_lowPass[s].b2 = ((1.0 - cosw0) / 2.0) / a0;
        m_lowPass[s].a1 = (-2.0 * cosw0) / a0;
        m_lowPass[s].a2 = (1.0 - alpha) / a0;
    }
}

void RBSCWDecoder::reset()
{
    m_time = 0.0;
    m_aaHistory.assign(std::max<std::size_t>(1, m_aaTaps.size()), 0.0f);
    m_aaIndex = 0;
    m_decimationCount = 0;
    m_fftInput.assign(m_fftSize, 0.0f);
    m_fftIndex = 0;
    m_fftHopCount = 0;
    m_ncoHz = m_settings.m_lockedToneHz;
    m_ncoPhase = 0.0;
    m_peakToneHz = -1.0f;
    m_peakSNR = 0.0;
    m_lastSignalTime = -100.0;
    m_candidateCount = 0;
    m_fftFrames = 0;
    m_candidateHz = 0.0f;
    m_averageSpectrum.clear();

    for (auto& section : m_lowPass) {
        section.z1 = section.z2 = 0.0;
    }

    m_envelopeSmooth = 0.0;
    m_envelopeCount = 0;
    m_envelopeSum = 0.0;
    m_levelHistory.assign(m_levelWindowMs, -200.0f);
    m_levelIndex = 0;
    m_levelFill = 0;
    m_levelUpdateCount = 0;
    m_noise = 1e-9;
    m_carrier = 1e-9;
    m_threshold = 1e9;
    m_peakEnvelope = 0.0;
    m_peakDecay = std::exp(-1.0 / 1500.0);
    m_rawMark = false;
    m_state = false;
    m_envelopeTime = 0;
    m_segmentStart = 0;
    m_pendingSince = -1;
    m_markHistory.clear();
    m_spaceHistory.clear();
    m_ditMs = m_settings.m_autoSpeed ? 60.0 : 1200.0 / std::max(5.0f, std::min(60.0f, m_settings.m_manualWPM));
    m_weightMs = 0.0;
    m_wordGapMs = 5.0 * m_ditMs;
    m_filterHz = 0.0f;
    updateFilter();
    m_elements.clear();
    m_elementErrors.clear();
    m_characterPending = false;
    m_wordSpacePending = false;
    m_acquiring = true;
    m_acquired.clear();
    m_markPeak = 0.0;
    m_characters.clear();
    m_scope.clear();
    m_scopeEnvelopeMax = 0.0f;
    m_scopeCount = 0;
}

void RBSCWDecoder::process(const float *samples, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (m_decimation == 1)
        {
            processDecimated(samples[i]);
            continue;
        }

        m_aaHistory[m_aaIndex] = samples[i];
        m_aaIndex = (m_aaIndex + 1) % (int) m_aaHistory.size();

        if (++m_decimationCount < m_decimation) {
            continue;
        }

        m_decimationCount = 0;
        // Circular history read in two straight runs: no modulo per tap
        double sum = 0.0;
        int size = (int) m_aaTaps.size();
        int split = size - m_aaIndex;

        for (int k = 0; k < split; k++) {
            sum += m_aaTaps[k] * m_aaHistory[m_aaIndex + k];
        }
        for (int k = split; k < size; k++) {
            sum += m_aaTaps[k] * m_aaHistory[k - split];
        }

        processDecimated((float) sum);
    }
}

void RBSCWDecoder::processDecimated(float sample)
{
    m_time += 1.0 / m_rate;

    // Tone tracking on the audio before the narrow filter
    m_fftInput[m_fftIndex] = sample;
    m_fftIndex = (m_fftIndex + 1) % m_fftSize;

    if (++m_fftHopCount >= m_fftHop)
    {
        m_fftHopCount = 0;
        trackTone();
    }

    // NCO down to baseband, then the narrow filter
    m_ncoPhase += 2.0 * m_pi * m_ncoHz / m_rate;

    if (m_ncoPhase > 2.0 * m_pi) {
        m_ncoPhase -= 2.0 * m_pi;
    }

    std::complex<double> baseband = (double) sample * std::complex<double>(std::cos(m_ncoPhase), -std::sin(m_ncoPhase));
    baseband = m_lowPass[1].run(m_lowPass[0].run(baseband));

    // Envelope, smoothed over about 5 ms, then 1 kHz
    double magnitude = 2.0 * std::abs(baseband);
    m_envelopeSmooth += m_envelopeAlpha * (magnitude - m_envelopeSmooth);
    m_envelopeSum += m_envelopeSmooth;

    if (++m_envelopeCount >= m_envelopeDecimation)
    {
        processEnvelope(m_envelopeSum / m_envelopeCount);
        m_envelopeSum = 0.0;
        m_envelopeCount = 0;
    }
}

// Sliding FFT (128 ms at 8 kHz) averaged over the last frames: the strongest bin of the tone
// range, compared with the median, tells if a tone is there and where. The NCO follows it
// smoothly; a clearly different tone seen 3 frames running is jumped to (new station)
void RBSCWDecoder::trackTone()
{
    std::vector<std::complex<double>> spectrum(m_fftSize);

    for (int i = 0; i < m_fftSize; i++) {
        spectrum[i] = m_fftInput[(m_fftIndex + i) % m_fftSize] * m_window[i];
    }

    fft(spectrum);
    int first = std::max(1, (int) std::ceil(m_settings.m_toneMinHz * m_fftSize / m_rate));
    int last = std::min(m_fftSize / 2 - 2, (int) std::floor(m_settings.m_toneMaxHz * m_fftSize / m_rate));

    if (last - first < 8) {
        return;
    }

    if ((int) m_averageSpectrum.size() != m_fftSize / 2) {
        m_averageSpectrum.assign(m_fftSize / 2, 0.0);
    }

    for (int k = first - 1; k <= last + 1; k++) {
        m_averageSpectrum[k] += 0.3 * (std::norm(spectrum[k]) - m_averageSpectrum[k]);
    }

    int peak = first;

    for (int k = first; k <= last; k++)
    {
        if (m_averageSpectrum[k] > m_averageSpectrum[peak]) {
            peak = k;
        }
    }

    std::vector<double> range(m_averageSpectrum.begin() + first, m_averageSpectrum.begin() + last + 1);
    std::nth_element(range.begin(), range.begin() + range.size() / 2, range.end());
    double median = std::max(1e-30, range[range.size() / 2]);
    m_peakSNR = m_averageSpectrum[peak] / median;

    // The average needs a few frames (about 0.3 s) before a peak in it means a tone
    if ((++m_fftFrames < 10) || (m_peakSNR < m_presenceRatio))
    {
        m_peakToneHz = -1.0f;
        m_candidateCount = 0;
        return;
    }

    // Peak between bins: parabola on the log powers
    double a = std::log(std::max(1e-30, m_averageSpectrum[peak - 1]));
    double b = std::log(std::max(1e-30, m_averageSpectrum[peak]));
    double c = std::log(std::max(1e-30, m_averageSpectrum[peak + 1]));
    double denominator = a - 2.0 * b + c;
    double shift = std::abs(denominator) > 1e-12 ? 0.5 * (a - c) / denominator : 0.0;
    float tone = (float) ((peak + std::max(-0.5, std::min(0.5, shift))) * m_rate / m_fftSize);
    m_peakToneHz = tone;
    m_lastSignalTime = m_time;

    if (m_settings.m_toneLocked)
    {
        m_ncoHz = m_settings.m_lockedToneHz;
        return;
    }

    if (std::abs(tone - m_ncoHz) <= std::max(25.0f, m_filterHz))
    {
        m_ncoHz += 0.2 * (tone - m_ncoHz);
        m_candidateCount = 0;
    }
    else
    {
        m_candidateCount = (m_candidateCount > 0) && (std::abs(tone - m_candidateHz) < 20.0f) ? m_candidateCount + 1 : 1;
        m_candidateHz = tone;

        if (m_candidateCount >= 3)
        {
            m_ncoHz = tone;
            m_candidateCount = 0;
        }
    }
}

// Noise and carrier: the 2 s of envelope (in dB) fall in two clusters when a station sends, found
// by a 1D k-means. The threshold is their geometric mean (the middle in dB)
void RBSCWDecoder::updateLevels()
{
    int count = m_levelFill;

    if (count < 100) {
        return;
    }

    float low = *std::min_element(m_levelHistory.begin(), m_levelHistory.begin() + count);
    float high = *std::max_element(m_levelHistory.begin(), m_levelHistory.begin() + count);

    for (int iteration = 0; iteration < 8; iteration++)
    {
        float boundary = 0.5f * (low + high);
        double lowSum = 0.0, highSum = 0.0;
        int lowCount = 0, highCount = 0;

        for (int i = 0; i < count; i++)
        {
            float v = m_levelHistory[i];

            if (v < boundary)
            {
                lowSum += v;
                lowCount++;
            }
            else
            {
                highSum += v;
                highCount++;
            }
        }

        if ((lowCount == 0) || (highCount == 0)) {
            break;
        }

        low = (float) (lowSum / lowCount);
        high = (float) (highSum / highCount);
    }

    // When a station starts, the 2 s still hold noise only: the carrier is then the peak of the
    // envelope (instant attack, slow decay), so that the threshold is right from the first mark
    float peakDB = (float) (20.0 * std::log10(m_peakEnvelope + 1e-12));

    if (peakDB - 1.0f > high) {
        high = peakDB - 1.0f;
    }

    // A clean signal has almost no noise: the noise level is taken no lower than 60 dB under the carrier
    low = std::max(low, high - 60.0f);
    m_noise = std::pow(10.0, low / 20.0);
    m_carrier = std::pow(10.0, high / 20.0);

    if (high - low < m_minLevelSpreadDB)
    {
        m_threshold = m_carrier * 2.0; // no carrier above the noise: nothing passes
    }
    else
    {
        // Geometric mean of noise and carrier, but no lower than half the carrier: on a strong signal
        // the threshold then sits at the middle of the edges, where the mark lengths are right
        double middleDB = 0.5 * (low + high);
        m_threshold = std::pow(10.0, std::max(middleDB, high - 6.0) / 20.0);
    }
}

void RBSCWDecoder::processEnvelope(double envelope)
{
    m_envelopeTime++;
    float envelopeDB = (float) (20.0 * std::log10(envelope + 1e-12));
    bool signal = (m_time - m_lastSignalTime) < m_presenceHoldS;

    // The first 300 ms are the filters settling: not a level
    if (m_envelopeTime > 300)
    {
        m_levelHistory[m_levelIndex] = envelopeDB;
        m_levelIndex = (m_levelIndex + 1) % m_levelWindowMs;
        m_levelFill = std::min(m_levelFill + 1, m_levelWindowMs);
    }

    // Carrier peak: follows a tone at once, decays in about 1.5 s
    if (signal && (envelope > m_peakEnvelope)) {
        m_peakEnvelope = envelope;
    } else {
        m_peakEnvelope *= m_peakDecay;
    }

    if (++m_levelUpdateCount >= m_levelUpdateMs)
    {
        m_levelUpdateCount = 0;
        updateLevels();
    }

    // Mark or space, with hysteresis, only while a tone is there

    if (!signal) {
        m_rawMark = false;
    } else if (m_rawMark) {
        m_rawMark = envelope > m_threshold / m_hysteresis;
    } else {
        m_rawMark = envelope > m_threshold * m_hysteresis;
    }

    // A change lasting less than 20 % of a dit is a glitch. And never shorter than what the narrow
    // filter lets through (1.5 / its width): else noise makes short marks, the dit estimate shrinks,
    // more noise passes, and the speed runs away
    double filterMinimum = 1500.0 / std::max(20.0f, m_filterHz);
    long long minimum = std::max((long long) std::lround(filterMinimum), (long long) std::lround(0.2 * m_ditMs));

    if (m_rawMark != m_state)
    {
        if (m_pendingSince < 0) {
            m_pendingSince = m_envelopeTime;
        }

        if (m_envelopeTime - m_pendingSince >= minimum)
        {
            segmentEnded(m_state, m_pendingSince - m_segmentStart);
            m_state = m_rawMark;
            m_markPeak = 0.0;
            m_segmentStart = m_pendingSince;
            m_pendingSince = -1;
        }
    }
    else
    {
        m_pendingSince = -1;
    }

    if (m_state) {
        m_markPeak = std::max(m_markPeak, envelope);
    } else {
        checkTimeouts();
    }

    // Scope: strongest envelope over 10 ms
    m_scopeEnvelopeMax = m_scopeCount == 0 ? envelopeDB : std::max(m_scopeEnvelopeMax, envelopeDB);

    if (++m_scopeCount >= 10)
    {
        m_scopeCount = 0;
        ScopePoint point;
        point.m_envelopeDB = m_scopeEnvelopeMax;
        point.m_thresholdDB = (float) (20.0 * std::log10(m_threshold + 1e-12));
        point.m_noiseDB = (float) (20.0 * std::log10(m_noise + 1e-12));
        point.m_carrierDB = (float) (20.0 * std::log10(m_carrier + 1e-12));
        point.m_mark = m_state;
        point.m_signal = signal;
        m_scope.push_back(point);

        if (m_scope.size() > 3000) {
            m_scope.erase(m_scope.begin(), m_scope.begin() + 1000);
        }
    }
}

void RBSCWDecoder::segmentEnded(bool mark, long long durationMs)
{
    if (durationMs <= 0) {
        return;
    }

    if (m_debug)
    {
        m_debugSegments.push_back(Segment{mark, (int) durationMs, (float) m_ditMs, (float) (20.0 * std::log10(m_threshold + 1e-12)),
            (float) (20.0 * std::log10(m_noise + 1e-12)), (float) (20.0 * std::log10(m_carrier + 1e-12))});
    }

    if (!m_acquiring)
    {
        addToHistory(mark, (double) durationMs);
        classify(mark, (double) durationMs);
        return;
    }

    float peakDB = mark ? (float) (20.0 * std::log10(m_markPeak + 1e-12)) : 0.0f;
    m_acquired.push_back(Acquired{mark, (double) durationMs, peakDB});
    addToHistory(mark, (double) durationMs);
    int marks = 0;

    for (const auto& segment : m_acquired) {
        marks += segment.m_mark ? 1 : 0;
    }

    if (marks >= 8) {
        endAcquisition();
    }
}

void RBSCWDecoder::addToHistory(bool mark, double durationMs)
{
    if (mark)
    {
        m_markHistory.push_back((float) durationMs);

        if ((int) m_markHistory.size() > m_markHistorySize) {
            m_markHistory.erase(m_markHistory.begin());
        }

        updateSpeed();
    }
    else if ((durationMs < 3000) && !m_markHistory.empty()) // between two marks, not before a station
    {
        m_spaceHistory.push_back((float) durationMs);

        if ((int) m_spaceHistory.size() > m_markHistorySize) {
            m_spaceHistory.erase(m_spaceHistory.begin());
        }
    }
}

// The marks and spaces held while acquiring, decoded with the speed found on them
void RBSCWDecoder::endAcquisition()
{
    m_acquiring = false;

    // Marks clearly weaker than the station's (noise before it started, a weaker station before
    // it) are dropped, with what came before them, and the speed found again without them
    std::vector<float> peaks;

    for (const auto& segment : m_acquired)
    {
        if (segment.m_mark) {
            peaks.push_back(segment.m_peakDB);
        }
    }

    std::size_t first = 0;

    if (peaks.size() >= 3)
    {
        std::nth_element(peaks.begin(), peaks.begin() + peaks.size() / 2, peaks.end());
        float station = peaks[peaks.size() / 2];

        for (std::size_t i = 0; i < m_acquired.size(); i++)
        {
            if (m_acquired[i].m_mark && (m_acquired[i].m_peakDB < station - 6.0f)) {
                first = i + 1;
            }
            if (m_acquired[i].m_mark && (m_acquired[i].m_peakDB >= station - 6.0f)) {
                break;
            }
        }
    }

    if (first > 0)
    {
        m_markHistory.clear();
        m_spaceHistory.clear();

        for (std::size_t i = first; i < m_acquired.size(); i++) {
            addToHistory(m_acquired[i].m_mark, m_acquired[i].m_ms);
        }
    }

    for (std::size_t i = first; i < m_acquired.size(); i++) {
        classify(m_acquired[i].m_mark, m_acquired[i].m_ms);
    }

    m_acquired.clear();
}

void RBSCWDecoder::flush()
{
    if (m_acquiring) {
        endAcquisition();
    }
    if (m_characterPending) {
        endCharacter();
    }
}

// A mark or space, its length compared with the dit
void RBSCWDecoder::classify(bool mark, double durationMs)
{
    // Weighting removed: marks shorter, spaces longer by it
    double corrected = mark ? durationMs - m_weightMs : durationMs + m_weightMs;

    if (mark)
    {
        // Far shorter than a dit once the speed is known: a noise spike, not an element
        if ((m_markHistory.size() >= 8) && (corrected < 0.35 * m_ditMs)) {
            return;
        }

        bool dah = corrected >= 2.0 * m_ditMs;
        double ideal = dah ? 3.0 * m_ditMs : m_ditMs;
        m_elements += dah ? '-' : '.';
        m_elementErrors.push_back((float) (std::abs(corrected - ideal) / ideal));
        m_characterPending = true;

        if (m_elements.size() > 9) {
            endCharacter(); // no character is that long: noise, start again
        }
    }
    else
    {
        if (m_characterPending)
        {
            if (corrected >= 2.0 * m_ditMs) {
                endCharacter(); // live, already done when the gap reached 2 dits
            } else {
                m_elementErrors.push_back((float) (std::abs(corrected - m_ditMs) / m_ditMs));
            }
        }

        if (m_wordSpacePending && (corrected >= m_wordGapMs))
        {
            m_characters.push_back(Character{" ", 1.0f, m_time});
            m_wordSpacePending = false;
        }
    }
}

// While the key is up: a gap of 2 dits ends the character, 5 dits the word. While acquiring, a long
// gap ends the acquisition (short message); a long silence starts a new one
void RBSCWDecoder::checkTimeouts()
{
    long long gapEnd = m_pendingSince >= 0 ? m_pendingSince : m_envelopeTime;
    double gap = (double) (gapEnd - m_segmentStart) + m_weightMs;

    if (m_acquiring)
    {
        if (!m_acquired.empty() && (gap >= std::max(1500.0, 10.0 * m_ditMs))) {
            endAcquisition();
        } else {
            return;
        }
    }

    if (!m_characterPending && (gap >= 3000.0))
    {
        m_acquiring = true;

        if (gap >= 6000.0)
        {
            // Probably another station: its speed from its own marks
            m_markHistory.clear();
            m_spaceHistory.clear();
        }
    }

    if (m_characterPending && (gap >= 2.0 * m_ditMs)) {
        endCharacter();
    }

    if (m_wordSpacePending && !m_characterPending && (gap >= m_wordGapMs))
    {
        m_characters.push_back(Character{" ", 1.0f, m_time});
        m_wordSpacePending = false;
    }
}

float RBSCWDecoder::snrScore() const
{
    double spread = 20.0 * std::log10(m_carrier / std::max(1e-12, m_noise));
    return (float) std::max(0.0, std::min(1.0, (spread - 6.0) / 14.0));
}

void RBSCWDecoder::endCharacter()
{
    if (m_elements.empty()) {
        m_characterPending = false;
        return;
    }

    std::string text = decodeElements(m_elements);
    double error = 0.0;

    for (float e : m_elementErrors) {
        error += std::min(1.0f, e);
    }

    error = m_elementErrors.empty() ? 0.0 : error / m_elementErrors.size();
    float timing = (float) (1.0 - error);
    float confidence = text == "<?>" ? 0.0f : std::max(0.0f, std::min(1.0f, timing * (0.4f + 0.6f * snrScore())));
    m_characters.push_back(Character{text, confidence, m_time});
    m_elements.clear();
    m_elementErrors.clear();
    m_characterPending = false;
    m_wordSpacePending = true;
}

// Dit u and weighting w that best explain the last marks and spaces. With a weighting, marks are
// longer than ideal and spaces shorter (real transmitters, the filters): a dit lasts u + w, a dah
// 3u + w, the gaps u - w (inside a character), 3u - w (between characters) and 7u - w or more
// (between words, longer with Farnsworth spacing). Each duration counts by its distance (in log)
// to the nearest of these, capped so that a glitch or a beacon's long dash weighs little. The
// spaces tell dits from dahs when all marks have one length ("5", "0", "EEEE"...)
void RBSCWDecoder::updateSpeed()
{
    if (!m_settings.m_autoSpeed)
    {
        m_ditMs = 1200.0 / std::max(5.0f, std::min(60.0f, m_settings.m_manualWPM));
        m_weightMs = 0.0;
        updateFilter();
        updateWordGap();
        return;
    }

    double dit, weight;

    if ((m_markHistory.size() < 3) || (m_spaceHistory.size() < 2))
    {
        // Too few to fit: each mark, as a dit or a dah, moves the estimate
        double mark = m_markHistory.back();
        dit = mark < 2.0 * m_ditMs ? mark : mark / 3.0;
        weight = 0.0;
    }
    else
    {
        static constexpr double cap = 0.35 * 0.35;  // a value off by more than 40 % counts as that
        std::vector<double> marks, spaces;

        for (float m : m_markHistory) {
            marks.push_back(std::log(m));
        }
        for (float s : m_spaceHistory) {
            spaces.push_back(std::log(s));
        }

        double bestCost = 1e30;
        dit = m_ditMs;
        weight = m_weightMs;

        for (double u = m_ditMinMs; u <= m_ditMaxMs; u *= 1.02)
        {
            for (int wi = -6; wi <= 10; wi++)
            {
                double w = wi * 0.05 * u;
                double markDit = std::log(u + w), markDah = std::log(3.0 * u + w);
                double gap1 = std::log(u - w), gap3 = std::log(3.0 * u - w), gap7 = std::log(7.0 * u - w);
                double cost = 0.0;

                for (double m : marks)
                {
                    double e = std::min(std::abs(m - markDit), std::abs(m - markDah));
                    cost += std::min(cap, e * e);
                }

                int insideGaps = 0;

                for (double s : spaces)
                {
                    double e1 = std::abs(s - gap1);
                    double e = s >= gap7 ? 0.0 : std::min(e1, std::min(std::abs(s - gap3), std::abs(s - gap7)));
                    cost += std::min(cap, e * e);
                    insideGaps += (e1 == e) && (e * e < cap) ? 1 : 0;
                }

                // Almost no gap inside a character means every mark is a character on its own:
                // rather a dit 3 times longer ("TTTTT" is "5" sent slowly)
                double fewInside = spaces.size() / 5.0 - insideGaps;

                if (fewInside > 0.0) {
                    cost += cap * fewInside;
                }

                // A slight preference for the current estimate, so that it does not jump around
                double drift = std::log(u / m_ditMs);
                cost += 0.02 * drift * drift + 0.01 * (wi * 0.05) * (wi * 0.05);

                if (cost < bestCost)
                {
                    bestCost = cost;
                    dit = u;
                    weight = w;
                }
            }
        }

        m_fitCost = bestCost / (cap * (marks.size() + spaces.size()));

        // Mostly noise: most durations fit no speed (clean Morse costs under 0.2, Morse at 0 dB
        // S/N about 0.35). Fitting it anyway, the speed runs away; the one found so far is kept
        if ((m_fitCost > 0.42) && (m_markHistory.size() >= 12)) {
            return;
        }
    }

    dit = std::max(m_ditMinMs, std::min(m_ditMaxMs, dit));

    // Faster while acquiring a new station, then steady
    double rate = m_markHistory.size() < 6 ? 0.6 : m_markHistory.size() < 20 ? 0.4 : 0.25;
    m_ditMs += rate * (dit - m_ditMs);
    m_weightMs += rate * (weight - m_weightMs);
    updateFilter();
    updateWordGap();
}

// Between characters 3 dits, between words 7: the word gap from 5 dits. With Farnsworth spacing
// both are stretched alike: from 1.5 times the gap between characters (median of the spaces from 2
// dits, most of them are between characters)
void RBSCWDecoder::updateWordGap()
{
    std::vector<double> long_;

    for (float s : m_spaceHistory)
    {
        double corrected = s + m_weightMs;

        if (corrected >= 2.0 * m_ditMs) {
            long_.push_back(corrected);
        }
    }

    double wordGap = 5.0 * m_ditMs;

    if (long_.size() >= 4)
    {
        std::nth_element(long_.begin(), long_.begin() + long_.size() / 2, long_.end());
        wordGap = std::max(wordGap, 1.5 * long_[long_.size() / 2]);
    }

    m_wordGapMs = wordGap;
}

std::vector<RBSCWDecoder::Character> RBSCWDecoder::takeCharacters()
{
    std::vector<Character> characters;
    characters.swap(m_characters);
    return characters;
}

std::vector<RBSCWDecoder::ScopePoint> RBSCWDecoder::takeScope()
{
    std::vector<ScopePoint> scope;
    scope.swap(m_scope);
    return scope;
}

RBSCWDecoder::Status RBSCWDecoder::getStatus() const
{
    Status status;
    bool signal = (m_time - m_lastSignalTime) < m_presenceHoldS;
    status.m_toneHz = (float) m_ncoHz;
    status.m_peakToneHz = signal ? m_peakToneHz : -1.0f;
    status.m_wpm = (float) (1200.0 / m_ditMs);
    status.m_snrDB = signal ? (float) (20.0 * std::log10(m_carrier / std::max(1e-12, m_noise))) : 0.0f;
    status.m_signal = signal;
    status.m_mark = m_state;
    return status;
}

// Morse tree as a table: letters, figures, punctuation, and the prosigns hams use (AR, SK, BT, KN,
// AS) where they share a code with punctuation
std::string RBSCWDecoder::decodeElements(const std::string& elements)
{
    static const std::map<std::string, std::string> codes = {
        {".-", "A"}, {"-...", "B"}, {"-.-.", "C"}, {"-..", "D"}, {".", "E"}, {"..-.", "F"}, {"--.", "G"},
        {"....", "H"}, {"..", "I"}, {".---", "J"}, {"-.-", "K"}, {".-..", "L"}, {"--", "M"}, {"-.", "N"},
        {"---", "O"}, {".--.", "P"}, {"--.-", "Q"}, {".-.", "R"}, {"...", "S"}, {"-", "T"}, {"..-", "U"},
        {"...-", "V"}, {".--", "W"}, {"-..-", "X"}, {"-.--", "Y"}, {"--..", "Z"},
        {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
        {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"}, {"----.", "9"},
        {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"}, {"-..-.", "/"}, {".----.", "'"},
        {"-.-.--", "!"}, {"---...", ":"}, {"-.-.-.", ";"}, {"-....-", "-"}, {"..--.-", "_"},
        {".-..-.", "\""}, {".--.-.", "@"}, {"-.--.-", ")"},
        {".-.-.", "<AR>"}, {"...-.-", "<SK>"}, {"-...-", "<BT>"}, {"-.--.", "<KN>"}, {".-...", "<AS>"},
        {"...-.", "<SN>"}, {"........", "<HH>"}
    };

    auto it = codes.find(elements);
    return it == codes.end() ? std::string("<?>") : it->second;
}
