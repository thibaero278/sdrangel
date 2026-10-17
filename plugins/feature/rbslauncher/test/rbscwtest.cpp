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

// Test bench of the RBS Morse decoder (RBSCWDecoder), on generated signals:
//   1. machine Morse, 20 WPM, strong
//   2. the same with white noise, signal to noise ratio going down (measured in 500 Hz)
//   3. speed going from 15 to 30 WPM in the message
//   4. hand sent: irregular timing, Farnsworth spacing
// For each one, the character error rate (edit distance to the text sent).
// "rbscwtest <folder>" also writes the test signals as WAV files there (mono 16 bit, 8 kHz), to
// play them into RBS through a virtual audio cable.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "../rbscwdecoder.h"

static const double m_pi = 3.14159265358979323846;

static const std::map<char, std::string>& morseTable()
{
    static const std::map<char, std::string> table = {
        {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."}, {'F', "..-."}, {'G', "--."},
        {'H', "...."}, {'I', ".."}, {'J', ".---"}, {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."},
        {'O', "---"}, {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"}, {'U', "..-"},
        {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"}, {'Z', "--.."},
        {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
        {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
        {'/', "-..-."}, {'?', "..--.."}, {'.', ".-.-.-"}, {',', "--..--"}
    };
    return table;
}

struct Keying
{
    double m_wpmStart = 20.0;    //!< Character speed at the start
    double m_wpmEnd = 20.0;      //!< And at the end (speed change)
    double m_farnsworthWpm = 0;  //!< Spacing speed if slower than the character speed, 0: none
    double m_jitter = 0.0;       //!< Relative random error on each element and gap (hand sending)
};

// Key down / up durations (s), alternating, starting with a key down
static std::vector<double> keyText(const std::string& text, const Keying& keying, std::mt19937& random)
{
    std::normal_distribution<double> normal(0.0, 1.0);
    std::vector<double> segments;
    auto jitter = [&](double duration) {
        return std::max(0.3 * duration, duration * (1.0 + keying.m_jitter * normal(random)));
    };
    auto addSpace = [&](double duration) {
        if (segments.empty()) {
            return;
        }
        if (segments.size() % 2 == 0) {
            segments.back() += duration;
        } else {
            segments.push_back(duration);
        }
    };

    for (std::size_t i = 0; i < text.size(); i++)
    {
        double progress = text.size() > 1 ? (double) i / (text.size() - 1) : 0.0;
        double wpm = keying.m_wpmStart + progress * (keying.m_wpmEnd - keying.m_wpmStart);
        double dit = 1.2 / wpm;
        double spacingDit = keying.m_farnsworthWpm > 0 ? 1.2 / keying.m_farnsworthWpm : dit;
        char c = text[i];

        if (c == ' ')
        {
            addSpace(jitter(4.0 * spacingDit)); // 7 dits with the 3 of the character gap
            continue;
        }

        auto code = morseTable().find(c);

        if (code == morseTable().end()) {
            continue;
        }

        for (std::size_t e = 0; e < code->second.size(); e++)
        {
            if (e > 0) {
                addSpace(jitter(dit));
            }

            segments.push_back(jitter(code->second[e] == '.' ? dit : 3.0 * dit));
        }

        addSpace(jitter(3.0 * spacingDit));
    }

    return segments;
}

// Tone keyed with 5 ms raised cosine edges, plus white noise. SNR in 500 Hz (signal power while
// the key is down over the noise power in 500 Hz)
static std::vector<float> makeSignal(const std::vector<double>& segments, int sampleRate, double toneHz, double snrDB,
    std::mt19937& random, double leadS = 1.0)
{
    std::vector<float> keyLevel;
    keyLevel.assign((std::size_t) (leadS * sampleRate), 0.0f);
    int edge = (int) (0.005 * sampleRate);

    for (std::size_t s = 0; s < segments.size(); s++)
    {
        int length = (int) std::lround(segments[s] * sampleRate);
        bool down = s % 2 == 0;

        for (int i = 0; i < length; i++)
        {
            float level = 0.0f;

            if (down)
            {
                double shape = 1.0;

                if (i < edge) {
                    shape = 0.5 - 0.5 * std::cos(m_pi * i / edge);
                } else if (i > length - edge) {
                    shape = 0.5 - 0.5 * std::cos(m_pi * (length - i) / edge);
                }

                level = (float) shape;
            }

            keyLevel.push_back(level);
        }
    }

    keyLevel.resize(keyLevel.size() + (std::size_t) (leadS * sampleRate), 0.0f);
    double amplitude = 0.25;
    double noiseSigma = std::isinf(snrDB) ? 0.0 :
        std::sqrt((amplitude * amplitude / 2.0) * sampleRate / (1000.0 * std::pow(10.0, snrDB / 10.0)));
    std::normal_distribution<double> noise(0.0, 1.0);
    std::vector<float> signal(keyLevel.size());

    for (std::size_t i = 0; i < keyLevel.size(); i++) {
        signal[i] = (float) (amplitude * keyLevel[i] * std::sin(2.0 * m_pi * toneHz * i / sampleRate) + noiseSigma * noise(random));
    }

    return signal;
}

static void writeWav(const std::string& path, const std::vector<float>& signal, int sampleRate)
{
    std::ofstream file(path, std::ios::binary);
    auto put32 = [&](uint32_t v) { file.write((const char*) &v, 4); };
    auto put16 = [&](uint16_t v) { file.write((const char*) &v, 2); };
    uint32_t dataSize = (uint32_t) (signal.size() * 2);
    file.write("RIFF", 4); put32(36 + dataSize); file.write("WAVE", 4);
    file.write("fmt ", 4); put32(16); put16(1); put16(1); put32(sampleRate); put32(sampleRate * 2); put16(2); put16(16);
    file.write("data", 4); put32(dataSize);

    for (float s : signal)
    {
        int v = (int) std::lround(std::max(-1.0f, std::min(1.0f, s)) * 32767.0f);
        put16((uint16_t) (int16_t) v);
    }
}

// Decoded text, prosigns and unknown characters as one character each
static std::string decode(const std::vector<float>& signal, int sampleRate, float filterHz, RBSCWDecoder::Status& status, float& meanConfidence)
{
    RBSCWDecoder decoder(sampleRate);
    RBSCWDecoder::Settings settings;
    settings.m_filterWidthHz = filterHz;
    decoder.setSettings(settings);
    decoder.reset();
    std::string text;
    double confidenceSum = 0.0;
    int confidenceCount = 0;
    const int block = sampleRate / 100;

    for (std::size_t i = 0; i < signal.size(); i += block)
    {
        decoder.process(signal.data() + i, (int) std::min<std::size_t>(block, signal.size() - i));

        if ((i / block) % 50 == 0) {
            status = decoder.getStatus();
        }

        for (const auto& c : decoder.takeCharacters())
        {
            text += c.m_text.size() == 1 ? c.m_text : std::string(c.m_text == "<?>" ? "#" : "*");

            if (c.m_text != " ")
            {
                confidenceSum += c.m_confidence;
                confidenceCount++;
            }
        }
    }

    meanConfidence = confidenceCount > 0 ? (float) (confidenceSum / confidenceCount) : 0.0f;
    return text;
}

static std::string normalize(const std::string& text)
{
    std::string result;

    for (char c : text)
    {
        if ((c == ' ') && (result.empty() || (result.back() == ' '))) {
            continue;
        }

        result += c;
    }

    while (!result.empty() && (result.back() == ' ')) {
        result.pop_back();
    }

    return result;
}

static double errorRate(const std::string& sent, const std::string& received)
{
    std::string a = normalize(sent), b = normalize(received);
    std::vector<std::vector<int>> d(a.size() + 1, std::vector<int>(b.size() + 1));

    for (std::size_t i = 0; i <= a.size(); i++) d[i][0] = (int) i;
    for (std::size_t j = 0; j <= b.size(); j++) d[0][j] = (int) j;

    for (std::size_t i = 1; i <= a.size(); i++) {
        for (std::size_t j = 1; j <= b.size(); j++) {
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        }
    }

    return a.empty() ? 0.0 : 100.0 * d[a.size()][b.size()] / a.size();
}

// "rbscwtest --diag <S/N dB>": the first marks and spaces as the decoder measures them
static int diagnose(double snrDB)
{
    const int sampleRate = 8000;
    std::mt19937 random(1234);
    Keying keying;
    std::vector<float> signal = makeSignal(keyText("CQ CQ CQ DE F4ABC F4ABC K", keying, random), sampleRate, 700.0, snrDB, random);
    RBSCWDecoder decoder(sampleRate);
    decoder.m_debug = true;
    std::string text;

    for (std::size_t i = 0; i < signal.size(); i += 80)
    {
        decoder.process(signal.data() + i, (int) std::min<std::size_t>(80, signal.size() - i));

        for (const auto& c : decoder.takeCharacters()) {
            text += c.m_text;
        }
    }

    for (std::size_t i = 0; i < std::min<std::size_t>(70, decoder.m_debugSegments.size()); i++)
    {
        const auto& s = decoder.m_debugSegments[i];
        std::printf("%s %4d ms  dit %5.1f  seuil %6.1f  bruit %6.1f  porteuse %6.1f dB\n", s.m_mark ? "MARK " : "space", s.m_ms, s.m_ditMs,
            s.m_thresholdDB, s.m_noiseDB, s.m_carrierDB);
    }

    std::printf("lu : %s\n", text.c_str());
    return 0;
}

int main(int argc, char *argv[])
{
    if ((argc > 2) && (std::string(argv[1]) == "--diag")) {
        return diagnose(std::atof(argv[2]));
    }

    const int sampleRate = 8000;
    const std::string wavFolder = argc > 1 ? argv[1] : "";
    const std::string text = "CQ CQ CQ DE F4ABC F4ABC K  F4ABC DE DL1XYZ UR RST 599 599 NAME HANS QTH BERLIN "
        "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 73 TU";
    std::mt19937 random(1234);
    int failures = 0;

    auto run = [&](const std::string& name, const Keying& keying, double snrDB, float filterHz, double maxErrorRate,
                   const std::string& wavName) {
        std::vector<double> segments = keyText(text, keying, random);
        std::vector<float> signal = makeSignal(segments, sampleRate, 700.0, snrDB, random);
        RBSCWDecoder::Status status;
        float confidence;
        std::string received = decode(signal, sampleRate, filterHz, status, confidence);
        double rate = errorRate(text, received);
        bool ok = rate <= maxErrorRate;
        failures += ok ? 0 : 1;
        std::printf("%-34s S/N %6s  filtre %3.0f Hz  erreurs %5.1f %%  vitesse lue %4.1f WPM  confiance %3.0f %%  %s\n",
            name.c_str(), std::isinf(snrDB) ? "inf" : std::to_string((int) snrDB).append(" dB").c_str(), filterHz, rate,
            status.m_wpm, 100.0f * confidence, ok ? "OK" : "ECHEC");

        if (!ok || (rate > 0.0)) {
            std::printf("    lu : %s\n", normalize(received).c_str());
        }
        if (!wavFolder.empty() && !wavName.empty()) {
            writeWav(wavFolder + "/" + wavName, signal, sampleRate);
        }
    };

    Keying machine;
    std::printf("1. Morse machine, 20 WPM\n");
    run("propre", machine, INFINITY, 100.0f, 0.0, "cw_20wpm_propre.wav");

    std::printf("\n2. Avec bruit blanc (S/N dans 500 Hz)\n");
    const int snrs[] = {20, 10, 6, 3, 0, -3, -6, -9, -12};

    for (int snr : snrs) {
        run("bruit", machine, snr, 100.0f, snr >= 0 ? 2.0 : 100.0, snr == 0 ? "cw_20wpm_bruit_0dB.wav" : snr == -6 ? "cw_20wpm_bruit_-6dB.wav" : "");
    }

    std::printf("\n   Filtre 50 Hz\n");

    for (int snr : {0, -6, -9, -12}) {
        run("bruit, filtre etroit", machine, snr, 50.0f, 100.0, "");
    }

    std::printf("\n3. Vitesse variable 15 -> 30 WPM\n");
    Keying variable;
    variable.m_wpmStart = 15.0;
    variable.m_wpmEnd = 30.0;
    run("vitesse variable", variable, INFINITY, 100.0f, 2.0, "cw_15-30wpm.wav");
    run("vitesse variable, bruit", variable, 6, 100.0f, 5.0, "");

    std::printf("\n4. Manipulation a la main\n");
    Keying hand;
    hand.m_wpmStart = hand.m_wpmEnd = 18.0;
    hand.m_jitter = 0.15;
    run("main, irregularite 15 %", hand, INFINITY, 100.0f, 3.0, "cw_main_18wpm.wav");
    hand.m_jitter = 0.25;
    run("main, irregularite 25 %", hand, INFINITY, 100.0f, 10.0, "");
    Keying farnsworth;
    farnsworth.m_wpmStart = farnsworth.m_wpmEnd = 20.0;
    farnsworth.m_farnsworthWpm = 10.0;
    run("Farnsworth 20/10 WPM", farnsworth, INFINITY, 100.0f, 2.0, "cw_farnsworth_20-10wpm.wav");
    Keying slow;
    slow.m_wpmStart = slow.m_wpmEnd = 8.0;
    run("lent, 8 WPM", slow, INFINITY, 100.0f, 2.0, "");
    Keying fast;
    fast.m_wpmStart = fast.m_wpmEnd = 40.0;
    run("rapide, 40 WPM", fast, 10, 250.0f, 2.0, "");

    std::printf("\n%s\n", failures == 0 ? "Tous les tests passent." : (std::to_string(failures) + " test(s) en echec.").c_str());
    return failures == 0 ? 0 : 1;
}
