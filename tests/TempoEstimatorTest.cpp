// Standalone tests for the audio TempoEstimator (no JUCE):
//   g++ -std=c++17 -I ../Source TempoEstimatorTest.cpp ../Source/TempoEstimator.cpp -o t && ./t

#include "TempoEstimator.h"

#include <cmath>
#include <cstdint>
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

    // Build a click track: short decaying noise burst on each beat at `bpm`.
    std::vector<float> clickTrack (double bpm, double seconds)
    {
        const int    total = (int) (kSR * seconds);
        std::vector<float> buf ((size_t) total, 0.0f);
        const double beat = 60.0 / bpm * kSR;      // samples per beat
        uint32_t rng = 12345;
        for (double pos = 0.0; pos < total; pos += beat)
        {
            const int start = (int) pos;
            for (int i = 0; i < 1200 && start + i < total; ++i)
            {
                rng = rng * 1664525u + 1013904223u;
                const float noise = (float) ((double) rng / 2147483648.0 - 1.0);
                const float env = std::exp (-i / 400.0f);   // fast decay
                buf[(size_t) (start + i)] += 0.7f * noise * env;
            }
        }
        return buf;
    }

    float estimate (double bpm)
    {
        TempoEstimator te;
        te.prepare (kSR);
        auto sig = clickTrack (bpm, 10.0);
        // Feed in 512-sample blocks like a host would.
        for (int i = 0; i + 512 <= (int) sig.size(); i += 512)
            te.processMono (sig.data() + i, 512);
        return te.getBpm();
    }
}

int main()
{
    for (double bpm : { 90.0, 100.0, 120.0, 128.0, 140.0, 160.0 })
    {
        const float got = estimate (bpm);
        std::printf ("click %.0f BPM -> estimated %.1f BPM\n", bpm, got);
        check (std::abs (got - bpm) <= 2.0, "estimates " + std::to_string ((int) bpm) + " BPM within 2");
    }

    // Steady tone (no beats) -> low confidence / no strong tempo.
    {
        TempoEstimator te; te.prepare (kSR);
        std::vector<float> tone ((size_t) (kSR * 6));
        for (int n = 0; n < (int) tone.size(); ++n)
            tone[(size_t) n] = 0.2f * std::sin (2.0 * M_PI * 220.0 * n / kSR);
        for (int i = 0; i + 512 <= (int) tone.size(); i += 512)
            te.processMono (tone.data() + i, 512);
        std::printf ("steady tone -> confidence %.2f\n", te.getConfidence());
        check (te.getConfidence() < 0.5f, "steady tone has low tempo confidence");
    }

    // "No beat": chords change at 110 BPM but the LOUDNESS is constant (no
    // amplitude onsets).  Spectral flux should still find the tempo.
    {
        const double bpm = 110.0;
        const double beat = 60.0 / bpm * kSR;
        const int total = (int) (kSR * 10);
        std::vector<float> sig ((size_t) total, 0.0f);
        // Two chords (C major / A minor-ish) alternating each beat.
        const double chordA[3] = { 261.63, 329.63, 392.00 };
        const double chordB[3] = { 293.66, 349.23, 440.00 };
        for (int n = 0; n < total; ++n)
        {
            const int beatIdx = (int) (n / beat);
            const double* ch = (beatIdx % 2 == 0) ? chordA : chordB;
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += std::sin (2.0 * M_PI * ch[k] * n / kSR);
            sig[(size_t) n] = (float) (0.2 * s);   // constant amplitude
        }
        TempoEstimator te; te.prepare (kSR);
        for (int i = 0; i + 512 <= total; i += 512)
            te.processMono (sig.data() + i, 512);
        std::printf ("no-beat chord changes 110 BPM -> %.1f BPM (conf %.2f)\n",
                     te.getBpm(), te.getConfidence());
        check (std::abs (te.getBpm() - 110.0) <= 3.0,
               "detects tempo from chord changes with no amplitude beat");
    }

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
