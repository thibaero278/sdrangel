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

#ifndef INCLUDE_FEATURE_RBSCWDECODER_H_
#define INCLUDE_FEATURE_RBSCWDECODER_H_

#include <complex>
#include <string>
#include <vector>

// Morse (CW) decoder for real, weak and hand sent signals. Plain C++ (no Qt, no SDRangel) so that
// it can be tested on its own (see test/rbscwtest.cpp).
//
// Chain, from audio (a CW tone of a few hundred Hz, as out of a USB / CW demodulator):
//  - decimation to about 8 kHz
//  - tone tracking: sliding FFT over the tone range, the NCO follows the peak smoothly (or is locked)
//  - NCO down to baseband, narrow low pass (4th order Butterworth, 50 to 500 Hz wide)
//  - envelope (magnitude), smoothed, at 1 kHz
//  - signal presence from the FFT peak, so that noise alone is not decoded
//  - adaptive threshold: noise and carrier levels over the last 2 s (two clusters of the
//    envelope), threshold at their geometric mean with hysteresis; a mark or space shorter
//    than 20 % of a dit is a glitch
//  - speed: the last 50 marks fall in two clusters, dits and dahs (1:3), and the shortest spaces
//    are the 1 dit gaps inside the characters (Farnsworth only stretches the longer ones). From
//    both, the dit and the weighting (marks longer and spaces shorter than ideal, as real
//    transmitters and the filters give); the dit length adapts progressively
//  - elements and gaps relative to the dit, weighting removed: dah from 2 dits, character gap from
//    2 dits, word gap from 5 dits
//  - a new station (after 3 s of silence) is held for its first 8 marks, then decoded with the
//    speed found on them, so that its first characters are right too
//  - Morse tree with figures, punctuation and prosigns; unknown sequences give "<?>"; each
//    character has a confidence from its timing errors and the signal to noise ratio
class RBSCWDecoder
{
public:
    struct Settings
    {
        float m_filterWidthHz = 0.0f;    //!< Narrow filter width: 50, 100, 250 or 500 Hz, 0 = follows the speed
        bool m_toneLocked = false;       //!< Keep the NCO on m_lockedToneHz
        float m_lockedToneHz = 650.0f;
        float m_toneMinHz = 200.0f;      //!< Tone search range: a sound card or WebSDR can give any pitch
        float m_toneMaxHz = 3000.0f;
        bool m_autoSpeed = true;         //!< Else the speed is m_manualWPM
        float m_manualWPM = 20.0f;
    };

    struct Character
    {
        std::string m_text;     //!< "A", "7", "<AR>", "<?>", or " " between words
        float m_confidence;     //!< 0 to 1
        double m_time;          //!< Seconds since the start, at the end of the character
    };

    struct Status
    {
        float m_toneHz;         //!< Where the NCO (filter centre) is
        float m_peakToneHz;     //!< Strongest tone seen by the FFT, < 0 if none
        float m_wpm;            //!< Estimated (or forced) speed
        float m_snrDB;          //!< Carrier over noise in the narrow filter, 0 if no signal
        bool m_signal;          //!< A tone is present
        bool m_mark;            //!< Key down now
    };

    // For the envelope scope, every 10 ms
    struct ScopePoint
    {
        float m_envelopeDB;     //!< Strongest envelope in the 10 ms
        float m_thresholdDB;
        float m_noiseDB;
        float m_carrierDB;
        bool m_mark;
        bool m_signal;
    };

    explicit RBSCWDecoder(int sampleRate = 8000);

    void setSampleRate(int sampleRate);
    void setSettings(const Settings& settings);
    const Settings& getSettings() const { return m_settings; }
    void reset();

    void process(const float *samples, int count);   //!< Audio, any amplitude

    void flush();                   //!< End of the audio: decode what is held
    std::vector<Character> takeCharacters();
    std::vector<ScopePoint> takeScope();
    Status getStatus() const;

    static std::string decodeElements(const std::string& elements);  //!< ".-" -> "A", "<?>" if unknown

    // For the test bench: marks (true) and spaces as measured, with the dit length then, ms
    struct Segment { bool m_mark; int m_ms; float m_ditMs; float m_thresholdDB; float m_noiseDB; float m_carrierDB; };
    std::vector<Segment> m_debugSegments;
    bool m_debug = false;

private:
    // Second order section, transposed direct form II
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::complex<double> z1, z2;
        std::complex<double> run(std::complex<double> x)
        {
            std::complex<double> y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    Settings m_settings;
    int m_sampleRate;
    int m_decimation;               //!< To about 8 kHz
    double m_rate;                  //!< After decimation
    double m_time;                  //!< Seconds of decimated samples

    // Decimation
    std::vector<float> m_aaTaps;
    std::vector<float> m_aaHistory;
    int m_aaIndex;
    int m_decimationCount;

    // Tone tracking
    static constexpr int m_fftSize = 1024;
    std::vector<float> m_fftInput;  //!< Last m_fftSize decimated samples, circular
    int m_fftIndex;
    int m_fftHopCount;
    std::vector<float> m_window;
    double m_ncoHz;
    double m_ncoPhase;
    float m_peakToneHz;
    double m_peakSNR;               //!< Peak over median in the tone range, linear power
    double m_lastSignalTime;        //!< Last time the FFT saw a tone clearly
    int m_candidateCount;
    int m_fftFrames;                //!< Since the start: the average is not reliable before a few
    std::vector<double> m_averageSpectrum;  //!< Power per bin, averaged over the last frames
    float m_candidateHz;

    // Narrow filter and envelope
    Biquad m_lowPass[2];
    double m_envelopeSmooth;
    double m_envelopeAlpha;         //!< Smoothing coefficient of the envelope (5 ms), from m_rate
    int m_envelopeDecimation;       //!< Decimated samples per envelope sample (1 kHz)
    int m_envelopeCount;
    double m_envelopeSum;

    // Levels over 2 s of envelope (1 kHz)
    std::vector<float> m_levelHistory;
    int m_levelIndex;
    int m_levelFill;
    int m_levelUpdateCount;
    double m_noise;
    double m_carrier;
    double m_threshold;
    double m_peakEnvelope;          //!< Carrier peak, instant attack
    double m_peakDecay;             //!< Per ms, for a decay in about 1.5 s

    // Marks and spaces, in envelope samples (ms)
    bool m_rawMark;
    bool m_state;                   //!< Debounced: mark or space
    long long m_envelopeTime;       //!< ms
    long long m_segmentStart;
    long long m_pendingSince;       //!< Start of a change not confirmed yet, -1 if none
    std::vector<float> m_markHistory;
    std::vector<float> m_spaceHistory;  //!< Spaces between marks, the shortest ones are 1 dit
    double m_ditMs;
    double m_weightMs;              //!< How much longer the marks are than ideal (and the spaces shorter)
    double m_wordGapMs;             //!< A space at least that long (weighting removed) ends a word
    double m_fitCost = 0.0;         //!< Of the last speed fit, 0 (perfect) to 1 (nothing fits)
    float m_filterHz;               //!< Narrow filter width in use (follows the speed when automatic)
    std::string m_elements;         //!< Of the character being received
    std::vector<float> m_elementErrors;
    bool m_characterPending;
    bool m_wordSpacePending;        //!< A word gap may follow the last character
    bool m_acquiring;               //!< New station: marks and spaces held until the speed is known
    struct Acquired { bool m_mark; double m_ms; float m_peakDB; };
    std::vector<Acquired> m_acquired;
    double m_markPeak;              //!< Strongest envelope of the mark going on

    std::vector<Character> m_characters;
    std::vector<ScopePoint> m_scope;
    float m_scopeEnvelopeMax;
    int m_scopeCount;

    void design();
    void processDecimated(float sample);
    void trackTone();
    void processEnvelope(double envelope);
    void updateLevels();
    void segmentEnded(bool mark, long long durationMs);
    void addToHistory(bool mark, double durationMs);
    void checkTimeouts();
    void classify(bool mark, double durationMs);
    void endAcquisition();
    void endCharacter();
    void updateSpeed();
    void updateFilter();
    void updateWordGap();
    float snrScore() const;
};

#endif // INCLUDE_FEATURE_RBSCWDECODER_H_
