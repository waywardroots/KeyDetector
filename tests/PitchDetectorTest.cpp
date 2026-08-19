// Standalone tests for the YIN PitchDetector (no JUCE):
//   g++ -std=c++17 -I ../Source PitchDetectorTest.cpp ../Source/PitchDetector.cpp -o t && ./t

#include "PitchDetector.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int failures = 0;
    void check (bool cond, const std::string& msg)
    {
        std::printf ("[%s] %s\n", cond ? "PASS" : "FAIL", msg.c_str());
        if (! cond) ++failures;
    }

    constexpr double kSR = 48000.0;
    constexpr int    kN  = 4096;

    // A slightly inharmonic sawtooth-ish tone (fundamental + a few harmonics) so the
    // test isn't a trivial single sinusoid.
    std::vector<float> makeTone (double freq)
    {
        std::vector<float> buf ((size_t) kN);
        for (int n = 0; n < kN; ++n)
        {
            double s = 0.0;
            for (int h = 1; h <= 5; ++h)
                s += (1.0 / h) * std::sin (2.0 * M_PI * freq * h * n / kSR);
            buf[(size_t) n] = (float) (0.2 * s);
        }
        return buf;
    }

    PitchDetector::Result detect (double freq)
    {
        PitchDetector pd;
        pd.prepare (kSR, kN);
        auto buf = makeTone (freq);
        return pd.process (buf.data(), kN);
    }
}

int main()
{
    struct Case { double freq; const char* note; };
    const Case cases[] = {
        {  82.41, "E2" }, // guitar low E
        { 110.00, "A2" },
        { 146.83, "D3" },
        { 220.00, "A3" },
        { 329.63, "E4" },
        { 440.00, "A4" },
        { 659.25, "E5" },
    };

    for (auto& c : cases)
    {
        auto r = detect (c.freq);
        int midi, pc, oct; double cents;
        PitchDetector::frequencyToNote (r.frequency, midi, pc, oct, cents);
        const std::string nm = PitchDetector::noteName (midi);
        std::printf ("%.2f Hz -> %.2f Hz  %s  %+.1f cents  clarity %.2f\n",
                     c.freq, r.frequency, nm.c_str(), cents, r.clarity);
        check (nm == c.note && std::abs (cents) < 10.0,
               std::string ("detects ") + c.note);
    }

    // A deliberately sharp A4 (+30 cents ~ 447.7 Hz) should read A4, ~+30 cents.
    {
        auto r = detect (440.0 * std::pow (2.0, 30.0 / 1200.0));
        int midi, pc, oct; double cents;
        PitchDetector::frequencyToNote (r.frequency, midi, pc, oct, cents);
        std::printf ("sharp A4 -> %s %+.1f cents\n", PitchDetector::noteName (midi).c_str(), cents);
        check (PitchDetector::noteName (midi) == "A4" && cents > 20.0 && cents < 40.0,
               "a +30 cent A4 reads as A4, ~+30 cents");
    }

    // Silence -> no pitch.
    {
        PitchDetector pd; pd.prepare (kSR, kN);
        std::vector<float> zeros ((size_t) kN, 0.0f);
        auto r = pd.process (zeros.data(), kN);
        check (r.frequency == 0.0f, "silence yields no pitch");
    }

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
