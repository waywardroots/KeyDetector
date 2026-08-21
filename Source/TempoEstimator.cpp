#include "TempoEstimator.h"

#include <algorithm>
#include <cmath>

void TempoEstimator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    onsetHop   = std::max (128, (int) std::round (newSampleRate / 90.0)); // ~90 Hz ODF rate
    onsetRate  = sampleRate / (double) onsetHop;

    // Keep enough history for a few of the slowest beats (60 BPM -> 1 s/beat).
    odfSize = std::max (256, (int) std::round (onsetRate * 8.0)); // ~8 s
    odf.assign ((size_t) odfSize, 0.0f);
    scratch.assign ((size_t) odfSize, 0.0f);

    reset();
}

void TempoEstimator::reset()
{
    std::fill (odf.begin(), odf.end(), 0.0f);
    writePos = 0; filled = 0; sinceCompute = 0;
    accum = 0.0; accumCount = 0; prevRms = 0.0f;
    bpm = 0.0f; confidence = 0.0f;
}

void TempoEstimator::pushOnset (float value)
{
    odf[(size_t) writePos] = value;
    writePos = (writePos + 1) % odfSize;
    if (filled < odfSize) ++filled;

    if (++sinceCompute >= computeEvery)
    {
        computeTempo();
        sinceCompute = 0;
    }
}

void TempoEstimator::processMono (const float* x, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        accum += (double) x[i] * x[i];
        if (++accumCount >= onsetHop)
        {
            const float rms = (float) std::sqrt (accum / accumCount);
            pushOnset (std::max (0.0f, rms - prevRms)); // half-wave-rectified rise
            prevRms   = rms;
            accum     = 0.0;
            accumCount = 0;
        }
    }
}

void TempoEstimator::computeTempo()
{
    const int minLag = std::max (2, (int) std::floor (onsetRate * 60.0 / maxBpm));
    const int maxLag = (int) std::ceil (onsetRate * 60.0 / minBpm);

    // Need at least two of the slowest beats to trust an estimate.
    if (filled < 2 * maxLag + 4)
        return;

    const int n = filled;

    // Linearise the ring into chronological order and remove the mean.
    double mean = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float v = odf[(size_t) ((writePos - n + i + odfSize) % odfSize)];
        scratch[(size_t) i] = v;
        mean += v;
    }
    mean /= n;

    double var = 0.0;
    for (int i = 0; i < n; ++i)
    {
        scratch[(size_t) i] -= (float) mean;
        var += (double) scratch[(size_t) i] * scratch[(size_t) i];
    }
    var /= n;
    if (var < 1.0e-12)
    {
        confidence = 0.0f;
        return;
    }

    // Normalised autocorrelation across the tempo lag range.
    auto autocorr = [&] (int lag) -> double
    {
        double acc = 0.0;
        for (int i = lag; i < n; ++i)
            acc += (double) scratch[(size_t) i] * scratch[(size_t) (i - lag)];
        return acc / ((double) (n - lag) * var);
    };

    int    bestLag = minLag;
    double bestScore = -1.0e300;

    // Perceptual tempo-preference weighting (log-Gaussian around ~120 BPM) resolves
    // the octave ambiguity, e.g. a 120 BPM pulse also correlates at 60 BPM.
    auto preference = [] (double b)
    {
        const double x = std::log2 (b / 120.0) / 0.7;
        return std::exp (-0.5 * x * x);
    };

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double b = 60.0 * onsetRate / lag;
        const double score = autocorr (lag) * preference (b);
        if (score > bestScore) { bestScore = score; bestLag = lag; }
    }

    const double bestVal = autocorr (bestLag);

    // Parabolic interpolation around the peak for sub-lag precision.
    double refinedLag = bestLag;
    if (bestLag > minLag && bestLag < maxLag)
    {
        const double a = autocorr (bestLag - 1);
        const double b = bestVal;
        const double c = autocorr (bestLag + 1);
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-12)
            refinedLag = bestLag + 0.5 * (a - c) / denom;
    }

    if (refinedLag > 0.0)
    {
        bpm        = (float) (60.0 * onsetRate / refinedLag);
        confidence = (float) std::clamp (bestVal, 0.0, 1.0);
    }
}
