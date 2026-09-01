// Key Detector - (c) 2026 JamesandtheCat. All rights reserved.
// This software is licensed, not sold; use is governed by the End User License
// Agreement (see EULA.md). Unauthorised copying, distribution, or modification is
// prohibited.

#pragma once

#include <array>
#include <vector>
#include <string>

/**
    ChromaKeyDetector
    -----------------
    Chroma-based musical key estimator.

    Signal pipeline (see the .cpp for the maths and the reasoning behind each step):

        magnitude spectrum ──► 12-bin chroma vector (pitch-class energy)
                           ──► Krumhansl–Schmuckler key-profile correlation
                           ──► best-matching major / minor key

    Design note:
    This class deliberately has NO JUCE dependency.  It only deals in plain
    magnitude arrays so the DSP can be unit-tested in isolation (see
    tests/ChromaKeyDetectorTest.cpp, which compiles with a bare C++17 compiler).
    The host / AudioProcessor is responsible for turning the incoming audio into a
    windowed FFT magnitude spectrum and feeding it here.
*/
class ChromaKeyDetector
{
public:
    static constexpr int numPitchClasses = 12;

    /** How the key (tonic) is chosen from the chroma vector. */
    enum class KeyMethod
    {
        Correlation,    // Krumhansl–Schmuckler key-profile correlation (24 keys)
        DominantPitch   // simply the loudest pitch class (its major key)
    };

    struct KeyEstimate
    {
        int   pitchClass  = 0;      // 0 = C, 1 = C#, 2 = D … 11 = B
        bool  isMinor     = false;  // false = major, true = minor
        float correlation = 0.0f;   // Pearson correlation with the winning profile (-1..1)
        float confidence  = 0.0f;   // normalised margin over the runner-up key (0..1)

        std::string name() const;      // e.g. "F# minor"
        std::string noteName() const;  // just the pitch class, e.g. "F#"  (no mode)
    };

    ChromaKeyDetector() = default;

    /** Prepare internal tables.  `fftSize` is the number of *real* input samples per
        analysis frame (i.e. bin k corresponds to k * sampleRate / fftSize Hz, and the
        caller passes fftSize/2 magnitudes to processSpectrum()). */
    void prepare (double sampleRate, int fftSize);

    /** Clears the accumulated chroma history (start estimating from scratch). */
    void reset();

    /** Feed one magnitude spectrum.  `numBins` should equal fftSize/2.  Updates the
        internally smoothed chroma vector (unless frozen or the frame is silent). */
    void processSpectrum (const float* magnitudes, int numBins);

    /** Exponential smoothing coefficient in [0, 1).  0 = react instantly to each
        frame, 0.9 = heavy smoothing.  Applied once per analysis frame. */
    void setSmoothing (float coeff) noexcept { smoothing = coeff; }

    /** While frozen, processSpectrum() is a no-op so the current result is held. */
    void setFrozen (bool shouldFreeze) noexcept { frozen = shouldFreeze; }

    /** When true, estimateKey() only considers the 12 major keys (never reports a
        minor key).  The GUI uses this so the readout is always a major key. */
    void setMajorOnly (bool shouldRestrict) noexcept { majorOnly = shouldRestrict; }

    /** Choose how the key is picked from the chroma (correlation vs loudest peak). */
    void setKeyMethod (KeyMethod m) noexcept { keyMethod = m; }

    /** The current smoothed, L1-normalised chroma vector (sums to ~1, or all-zero
        before any non-silent frame has been seen). */
    std::array<float, numPitchClasses> getChroma() const;

    /** Correlate the current chroma against all 24 key profiles and return the best. */
    KeyEstimate estimateKey() const;

    /** Like estimateKey(), but with hysteresis: the returned key only changes once a
        new candidate has been the instantaneous winner for `keyHoldTime` seconds of
        continuous frames.  `frameSeconds` is the time between analysis frames (hop
        size / sample rate).  This is what the plugin uses so the readout is steady. */
    KeyEstimate estimateStableKey (double frameSeconds);

    /** How long a new key must persist before it is reported (default 0.6 s). */
    void setKeyHoldTime (float seconds) noexcept { keyHoldSeconds = seconds; }

    /** Exact equal-tempered note frequencies (Hz, A4 = 440).  Row = pitch class
        (0 = C … 11 = B), column = octave (0..8).  These are the reference
        frequencies used to assign each FFT bin to a pitch class. */
    static const double referenceNoteFrequencies[numPitchClasses][9];

private:
    // Snap a frequency to the nearest note in referenceNoteFrequencies (A440 grid).
    // Returns the pitch class (0..11) and the deviation from that note in cents.
    static void nearestNote (double freqHz, int& pitchClass, double& cents);

    // Pearson correlation of the current chroma with one key profile.
    double keyCorrelation (int key, bool minor) const;

    std::array<double, numPitchClasses> chroma {}; // smoothed accumulator
    double sampleRate = 44100.0;
    int    fftSize    = 4096;
    float  smoothing  = 0.85f;
    bool   frozen     = false;
    bool   majorOnly  = false;
    KeyMethod keyMethod = KeyMethod::Correlation;

    // Key hysteresis state (used by estimateStableKey).
    int    stablePc      = -1;
    bool   stableMinor   = false;
    int    candPc        = -1;
    bool   candMinor     = false;
    int    candCount     = 0;
    float  keyHoldSeconds = 0.6f;

    // Analysis band.  We ignore sub-bass rumble below ~A1 and content above ~5 kHz;
    // the latter mostly holds high harmonics/noise that add little to pitch-class
    // discrimination.  Harmonics of low notes still land inside the band and reinforce
    // the correct pitch classes, which is what makes chroma robust at low resolution.
    static constexpr double fMin = 55.0;    // ~A1
    static constexpr double fMax = 5000.0;
};
