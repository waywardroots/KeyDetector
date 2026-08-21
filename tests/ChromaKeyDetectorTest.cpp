// Standalone sanity tests for ChromaKeyDetector.
// Compiles with a bare C++17 compiler (no JUCE required):
//
//   g++ -std=c++17 -I ../Source ChromaKeyDetectorTest.cpp ../Source/ChromaKeyDetector.cpp -o test && ./test
//
// The tests synthesise magnitude spectra and check that the estimated key matches
// what we put in.

#include "ChromaKeyDetector.h"

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

    constexpr double kSampleRate = 48000.0;
    constexpr int    kFftSize    = 8192;   // matches the plugin (fine enough to resolve semitones)
    constexpr int    kNumBins    = kFftSize / 2;

    // Add a peak (with a couple of harmonics) at a given frequency into a magnitude
    // spectrum, spreading a little energy into neighbouring bins like a real window.
    void addTone (std::vector<float>& mags, double freqHz, float amp)
    {
        for (int harmonic = 1; harmonic <= 4; ++harmonic)
        {
            const double f   = freqHz * harmonic;
            const double bin = f * kFftSize / kSampleRate;
            const int    b   = (int) std::lround (bin);
            const float  a   = amp / (float) harmonic; // 1/n harmonic roll-off

            for (int d = -1; d <= 1; ++d)
            {
                const int idx = b + d;
                if (idx > 0 && idx < kNumBins)
                    mags[(size_t) idx] += a * (d == 0 ? 1.0f : 0.4f);
            }
        }
    }

    // Feed a chord (set of frequencies) repeatedly so the EMA settles.
    ChromaKeyDetector::KeyEstimate detect (const std::vector<double>& freqs)
    {
        ChromaKeyDetector det;
        det.prepare (kSampleRate, kFftSize);
        det.setSmoothing (0.5f);

        for (int frame = 0; frame < 50; ++frame)
        {
            std::vector<float> mags ((size_t) kNumBins, 0.0f);
            for (double f : freqs)
                addTone (mags, f, 1.0f);
            det.processSpectrum (mags.data(), kNumBins);
        }

        return det.estimateKey();
    }
}

int main()
{
    // --- 1) Feeding a key profile directly must recover that key exactly ---------
    {
        // C major profile placed straight into the chroma via a synthetic spectrum
        // is hard to do without an FFT, so instead we test the correlation path by
        // constructing a spectrum from the C-major *scale* and checking the tonic.
        std::vector<double> cMajorScale = {
            261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, // C4 D4 E4 F4 G4 A4 B4
            523.25                                                    // C5
        };
        auto est = detect (cMajorScale);
        std::printf ("C major scale -> %s (r=%.3f, conf=%.2f)\n",
                     est.name().c_str(), est.correlation, est.confidence);
        check (est.pitchClass == 0 && ! est.isMinor, "C major scale is detected as C major");
    }

    // --- 2) A natural-minor scale should resolve to the minor tonic --------------
    {
        std::vector<double> aMinorScale = {
            220.00, 246.94, 261.63, 293.66, 329.63, 349.23, 392.00, // A3 B3 C4 D4 E4 F4 G4
            440.00                                                    // A4
        };
        auto est = detect (aMinorScale);
        std::printf ("A minor scale -> %s (r=%.3f, conf=%.2f)\n",
                     est.name().c_str(), est.correlation, est.confidence);
        check (est.pitchClass == 9 && est.isMinor, "A minor scale is detected as A minor");
    }

    // --- 3) A transposed major scale (G major) should track the transposition -----
    {
        std::vector<double> gMajorScale = {
            196.00, 220.00, 246.94, 261.63, 293.66, 329.63, 369.99, // G3 A3 B3 C4 D4 E4 F#4
            392.00                                                    // G4
        };
        auto est = detect (gMajorScale);
        std::printf ("G major scale -> %s (r=%.3f, conf=%.2f)\n",
                     est.name().c_str(), est.correlation, est.confidence);
        check (est.pitchClass == 7 && ! est.isMinor, "G major scale is detected as G major");
    }

    // --- 4) Bin -> pitch-class mapping uses the supplied A4 = 440 reference -------
    {
        ChromaKeyDetector det;
        det.prepare (kSampleRate, kFftSize);
        std::vector<float> mags ((size_t) kNumBins, 0.0f);
        // A single pure A (440 Hz, no harmonics) must light up pitch class 9 (A).
        const int aBin = (int) std::lround (440.0 * kFftSize / kSampleRate);
        mags[(size_t) aBin] = 1.0f;
        det.setSmoothing (0.0f);
        det.processSpectrum (mags.data(), kNumBins);
        auto chroma = det.getChroma();
        int argmax = 0;
        for (int i = 1; i < 12; ++i)
            if (chroma[(size_t) i] > chroma[(size_t) argmax]) argmax = i;
        check (argmax == 9, "440 Hz maps to pitch class A (index 9)");
    }

    // --- 5) Major-only mode (used by the GUI) never reports a minor key ----------
    {
        ChromaKeyDetector det;
        det.prepare (kSampleRate, kFftSize);
        det.setSmoothing (0.5f);
        det.setMajorOnly (true);

        // A natural-minor scale shares its notes with the relative major (C major),
        // so in major-only mode it must resolve to C major, and never to a minor key.
        // (Single octave, no doubled tonic, so there is no A emphasis to bias it.)
        const std::vector<double> aMinorScale = {
            220.00, 246.94, 261.63, 293.66, 329.63, 349.23, 392.00
        };
        for (int frame = 0; frame < 50; ++frame)
        {
            std::vector<float> mags ((size_t) kNumBins, 0.0f);
            for (double f : aMinorScale)
                addTone (mags, f, 1.0f);
            det.processSpectrum (mags.data(), kNumBins);
        }
        auto est = det.estimateKey();
        std::printf ("A-minor notes (major-only) -> %s (r=%.3f)\n", est.name().c_str(), est.correlation);
        check (! est.isMinor, "major-only mode never reports a minor key");
        check (est.pitchClass == 0, "A-minor notes resolve to relative major (C) in major-only mode");
    }

    // --- 6) Key hysteresis: a momentary winner must not flip the reported key ----
    {
        ChromaKeyDetector det;
        det.prepare (kSampleRate, kFftSize);
        det.setSmoothing (0.0f);       // react instantly so we control the chroma directly
        det.setKeyHoldTime (0.5f);     // require 0.5 s of persistence to switch

        const double frameSeconds = 0.043; // ~ plugin hop time
        const int    needed = (int) std::lround (0.5 / frameSeconds); // ~12 frames

        auto feed = [&] (const std::vector<double>& notes)
        {
            std::vector<float> mags ((size_t) kNumBins, 0.0f);
            for (double f : notes)
                addTone (mags, f, 1.0f);
            det.processSpectrum (mags.data(), kNumBins);
        };

        const std::vector<double> cChord = { 261.63, 329.63, 392.00 };            // C major
        const std::vector<double> gChord = { 196.00, 246.94, 293.66, 392.00 };    // G major

        // Establish C as the stable key.
        for (int i = 0; i < 30; ++i) { feed (cChord); det.estimateStableKey (frameSeconds); }
        const bool startedC = (det.estimateStableKey (frameSeconds).pitchClass == 0);

        // A single G frame must NOT change the reported key.
        feed (gChord);
        const bool heldAfterBlip = (det.estimateStableKey (frameSeconds).pitchClass == 0);

        // A sustained run of G frames SHOULD eventually switch it to G.
        bool switched = false;
        for (int i = 0; i < needed + 5; ++i)
        {
            feed (gChord);
            if (det.estimateStableKey (frameSeconds).pitchClass == 7) { switched = true; break; }
        }

        std::printf ("hysteresis: startedC=%d heldAfterBlip=%d switched=%d\n",
                     startedC, heldAfterBlip, switched);
        check (startedC,       "hysteresis establishes the initial (C) key");
        check (heldAfterBlip,  "a single-frame G blip does not flip the reported key");
        check (switched,       "a sustained G run does switch the reported key");
    }

    // --- 7) Dominant-pitch method: the loudest pitch class becomes the key -------
    {
        ChromaKeyDetector det;
        det.prepare (kSampleRate, kFftSize);
        det.setSmoothing (0.5f);
        det.setKeyMethod (ChromaKeyDetector::KeyMethod::DominantPitch);

        // E is by far the loudest note; a couple of quiet others are present too.
        for (int frame = 0; frame < 40; ++frame)
        {
            std::vector<float> mags ((size_t) kNumBins, 0.0f);
            addTone (mags, 329.63, 1.0f);  // E4  (loud)
            addTone (mags, 261.63, 0.25f); // C4  (quiet)
            addTone (mags, 392.00, 0.25f); // G4  (quiet)
            det.processSpectrum (mags.data(), kNumBins);
        }
        auto est = det.estimateKey();
        std::printf ("dominant-pitch (E loudest) -> %s (strength=%.2f)\n",
                     est.name().c_str(), est.correlation);
        check (est.pitchClass == 4 && ! est.isMinor,
               "dominant-pitch method picks the loudest pitch class (E) as the key");
    }

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
