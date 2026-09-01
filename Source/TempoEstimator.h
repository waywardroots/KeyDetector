// Key Detector - (c) 2026 JamesandtheCat. All rights reserved.
// This software is licensed, not sold; use is governed by the End User License
// Agreement (see EULA.md). Unauthorised copying, distribution, or modification is
// prohibited.

#pragma once

#include <vector>

/**
    TempoEstimator
    --------------
    Estimates tempo (BPM) directly from the audio, independent of the host clock.

    Pipeline:
      1. Onset-detection function (ODF) = **spectral flux**: the audio is chopped
         into short overlapping windows; the ODF is the sum of the positive changes
         in the magnitude spectrum between consecutive windows.  Spectral flux peaks
         both on drum hits *and* on note/chord changes at constant loudness, so a
         tempo can be found even when there is no percussive "beat".
      2. The ODF history is mean-removed and auto-correlated over the lag range for a
         musical tempo range.  A gentle log-Gaussian tempo preference (~120 BPM)
         nudges the octave choice.  The strongest periodicity -> beat period -> BPM.
      3. A normalised peak height is reported as a confidence.

    No JUCE dependency (has a tiny internal radix-2 FFT), so it is unit-testable.
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
    void processHop();        // compute spectral flux for the current window
    void pushOnset (float value);
    void computeTempo();

    double sampleRate = 44100.0;

    // Spectral-flux front end.
    int    fftSize   = 4096;
    int    hop       = 512;
    double onsetRate = 93.75; // ODF samples per second (sampleRate / hop)

    std::vector<float> inRing;      // last fftSize input samples (circular)
    int    inPos = 0;
    int    hopCountdown = 0;

    std::vector<float> hann;        // window
    std::vector<float> re, im;      // FFT scratch
    std::vector<float> prevMag;     // previous magnitude spectrum
    int    fluxMaxBin = 0;          // ignore very high bins

    // ODF ring buffer.
    std::vector<float> odf;
    int    odfSize    = 1024;
    int    writePos   = 0;
    int    filled     = 0;
    int    sinceCompute = 0;
    int    computeEvery = 16;

    double minBpm = 60.0;
    double maxBpm = 200.0;

    float  bpm        = 0.0f;
    float  confidence = 0.0f;

    std::vector<float> scratch;     // linearised, mean-removed ODF window
};
