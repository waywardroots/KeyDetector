#pragma once

#include <vector>

/**
    TempoEstimator
    --------------
    Estimates tempo (BPM) directly from the audio, independent of the host clock.

    Pipeline:
      1. Onset-detection function (ODF): the audio is chopped into short hops; for
         each hop we take its RMS and keep the *rise* in level (half-wave rectified
         difference).  This peaks at note/drum onsets.
      2. The ODF history is mean-removed and auto-correlated over the lag range that
         corresponds to a musical tempo range (default 60–200 BPM).  The strongest
         periodicity (with parabolic interpolation) gives the beat period -> BPM.
      3. A normalised peak height is reported as a confidence, so the caller can hide
         the read-out when there is no clear pulse (e.g. sustained/among-tonal pads).

    No JUCE dependency, so it can be unit-tested on its own.
*/
class TempoEstimator
{
public:
    TempoEstimator() = default;

    void prepare (double sampleRate);
    void reset();

    /** Feed mono audio (call once per processBlock). */
    void processMono (const float* samples, int numSamples);

    /** Latest tempo estimate in BPM (0 if none yet) and its confidence (0..1). */
    float getBpm()        const noexcept { return bpm; }
    float getConfidence() const noexcept { return confidence; }

    void setTempoRange (double minBpmIn, double maxBpmIn) noexcept
    {
        minBpm = minBpmIn; maxBpm = maxBpmIn;
    }

private:
    void pushOnset (float value);
    void computeTempo();

    double sampleRate = 44100.0;
    int    onsetHop   = 512;      // samples per ODF sample
    double onsetRate  = 86.13;    // ODF samples per second (sampleRate / onsetHop)

    // Per-hop RMS accumulation.
    double accum      = 0.0;
    int    accumCount = 0;
    float  prevRms    = 0.0f;

    // ODF ring buffer.
    std::vector<float> odf;
    int    odfSize    = 1024;     // ~11 s at 48 kHz / hop 512
    int    writePos   = 0;
    int    filled     = 0;
    int    sinceCompute = 0;
    int    computeEvery = 16;     // recompute tempo every ~0.17 s

    double minBpm = 60.0;
    double maxBpm = 200.0;

    float  bpm        = 0.0f;
    float  confidence = 0.0f;

    std::vector<float> scratch;   // linearised, mean-removed ODF window
};
