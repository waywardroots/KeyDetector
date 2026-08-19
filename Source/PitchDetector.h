#pragma once

#include <string>
#include <vector>

/**
    PitchDetector
    -------------
    Monophonic fundamental-frequency estimator using the YIN algorithm
    (de Cheveigné & Kawahara, 2002).  YIN is a time-domain autocorrelation-style
    method that is accurate to a few cents and robust against octave errors, which
    is exactly what an instrument tuner needs.

    It also reports a "clarity" value (1 - aperiodicity): high for a clean single
    note, low for chords / noise / silence.  The plugin uses that to only show a
    tuning reading when a single note is actually being played, so the same input
    can drive both the (polyphonic) key detector and the (monophonic) tuner.

    No JUCE dependency, so it can be unit-tested on its own.
*/
class PitchDetector
{
public:
    struct Result
    {
        float frequency = 0.0f;  // Hz, or 0 if no confident pitch was found
        float clarity   = 0.0f;  // 0..1, higher = more clearly periodic
    };

    PitchDetector() = default;

    /** Allocate working buffers.  maxBufferSize is the largest sample count that
        will be passed to process() (the analysis window is maxBufferSize/2). */
    void prepare (double sampleRate, int maxBufferSize);

    /** Estimate the pitch of `numSamples` mono samples (uses numSamples/2 as the
        integration window).  Returns {0,0} if there is no confident pitch. */
    Result process (const float* samples, int numSamples);

    /** Lowest / highest fundamentals the tuner will report. */
    void setFrequencyRange (double minHz, double maxHz) noexcept { minFreq = minHz; maxFreq = maxHz; }

    /** Aperiodicity threshold used by YIN's absolute-threshold step (default 0.15). */
    void setThreshold (float t) noexcept { threshold = t; }

    //==============================================================================
    // Note-name helpers (equal temperament, A4 = 440 Hz, scientific pitch notation).

    /** Decompose a frequency into the nearest MIDI note, pitch class (0=C..11=B),
        octave, and the signed deviation from that note in cents. */
    static void frequencyToNote (double hz, int& midiNote, int& pitchClass,
                                 int& octave, double& cents);

    /** e.g. midi 69 -> "A4". */
    static std::string noteName (int midiNote);

private:
    double sampleRate = 44100.0;
    double minFreq    = 40.0;
    double maxFreq    = 1500.0;
    float  threshold  = 0.15f;

    std::vector<float> diff;   // YIN difference function d(tau)
    std::vector<float> cmnd;   // cumulative mean normalised difference d'(tau)
    int maxTau = 0;
};
