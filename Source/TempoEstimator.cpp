// Key Detector - a JamesandtheCat plug-in.  (c) 2026 AudioFuzz. All rights reserved.
// This software is licensed, not sold; use is governed by the End User License
// Agreement (see EULA.md). Unauthorised copying, distribution, or modification is
// prohibited.

#include "TempoEstimator.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    // In-place iterative radix-2 Cooley–Tukey FFT.
    void fft (std::vector<float>& re, std::vector<float>& im)
    {
        const int n = (int) re.size();

        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { std::swap (re[(size_t) i], re[(size_t) j]);
                         std::swap (im[(size_t) i], im[(size_t) j]); }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const double ang = -2.0 * kPi / len;
            const float wr = (float) std::cos (ang);
            const float wi = (float) std::sin (ang);
            for (int i = 0; i < n; i += len)
            {
                float cr = 1.0f, ci = 0.0f;
                for (int k = 0; k < len / 2; ++k)
                {
                    const int a = i + k, b = i + k + len / 2;
                    const float xr = re[(size_t) b] * cr - im[(size_t) b] * ci;
                    const float xi = re[(size_t) b] * ci + im[(size_t) b] * cr;
                    re[(size_t) b] = re[(size_t) a] - xr;
                    im[(size_t) b] = im[(size_t) a] - xi;
                    re[(size_t) a] += xr;
                    im[(size_t) a] += xi;
                    const float ncr = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr;
                    cr = ncr;
                }
            }
        }
    }
}

void TempoEstimator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fftSize    = 4096;   // finer bins -> steady tones leak far less (fewer false onsets)
    hop        = 512;
    onsetRate  = sampleRate / (double) hop;

    inRing.assign ((size_t) fftSize, 0.0f);
    re.assign     ((size_t) fftSize, 0.0f);
    im.assign     ((size_t) fftSize, 0.0f);
    prevMag.assign ((size_t) (fftSize / 2), 0.0f);

    hann.assign ((size_t) fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
        hann[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * i / (fftSize - 1));

    // Spectral flux up to ~8 kHz is plenty for onset/rhythm content.
    fluxMaxBin = std::min (fftSize / 2, (int) (8000.0 / (sampleRate / fftSize)));

    odfSize = std::max (256, (int) std::round (onsetRate * 8.0));
    odf.assign ((size_t) odfSize, 0.0f);
    scratch.assign ((size_t) odfSize, 0.0f);

    reset();
}

void TempoEstimator::reset()
{
    std::fill (inRing.begin(), inRing.end(), 0.0f);
    std::fill (prevMag.begin(), prevMag.end(), 0.0f);
    std::fill (odf.begin(), odf.end(), 0.0f);
    inPos = 0; hopCountdown = hop;
    writePos = 0; filled = 0; sinceCompute = 0;
    bpm = 0.0f; confidence = 0.0f;
}

void TempoEstimator::processMono (const float* x, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        inRing[(size_t) inPos] = x[i];
        inPos = (inPos + 1) % fftSize;
        if (--hopCountdown <= 0)
        {
            processHop();
            hopCountdown = hop;
        }
    }
}

void TempoEstimator::processHop()
{
    // Windowed FFT of the last fftSize samples (oldest first).
    for (int i = 0; i < fftSize; ++i)
    {
        re[(size_t) i] = inRing[(size_t) ((inPos + i) % fftSize)] * hann[(size_t) i];
        im[(size_t) i] = 0.0f;
    }
    fft (re, im);

    // Spectral flux = sum of positive magnitude changes (onset strength).
    float flux = 0.0f;
    for (int k = 1; k < fluxMaxBin; ++k)
    {
        const float mag = std::sqrt (re[(size_t) k] * re[(size_t) k]
                                   + im[(size_t) k] * im[(size_t) k]);
        const float d = mag - prevMag[(size_t) k];
        if (d > 0.0f) flux += d;
        prevMag[(size_t) k] = mag;
    }

    pushOnset (flux);
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

void TempoEstimator::computeTempo()
{
    const int minLag = std::max (2, (int) std::floor (onsetRate * 60.0 / maxBpm));
    const int maxLag = (int) std::ceil (onsetRate * 60.0 / minBpm);

    if (filled < 2 * maxLag + 4)
        return;

    const int n = filled;

    double mean = 0.0, maxRaw = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float v = odf[(size_t) ((writePos - n + i + odfSize) % odfSize)];
        scratch[(size_t) i] = v;
        mean += v;
        if (v > maxRaw) maxRaw = v;
    }
    mean /= n;

    // Onset-activity (crest) gate: real rhythm / note-changes give a spiky onset
    // envelope (peaks far above the mean); steady/sustained material gives a flat
    // one.  Rejecting low-crest frames avoids reporting a tempo when there is no
    // real onset activity.
    const double crest = maxRaw / (mean + 1.0e-12);
    if (crest < 4.0)
    {
        confidence = 0.0f;
        return;
    }

    double var = 0.0;
    for (int i = 0; i < n; ++i)
    {
        scratch[(size_t) i] -= (float) mean;
        var += (double) scratch[(size_t) i] * scratch[(size_t) i];
    }
    var /= n;
    if (var < 1.0e-12) { confidence = 0.0f; return; }

    auto autocorr = [&] (int lag) -> double
    {
        double acc = 0.0;
        for (int i = lag; i < n; ++i)
            acc += (double) scratch[(size_t) i] * scratch[(size_t) (i - lag)];
        return acc / ((double) (n - lag) * var);
    };

    // Gentle tempo preference (log-Gaussian around ~120 BPM) to bias the octave.
    auto preference = [] (double b)
    {
        const double x = std::log2 (b / 120.0) / 1.0;
        return std::exp (-0.5 * x * x);
    };

    int    bestLag = minLag;
    double bestScore = -1.0e300;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double b = 60.0 * onsetRate / lag;
        const double score = autocorr (lag) * preference (b);
        if (score > bestScore) { bestScore = score; bestLag = lag; }
    }

    const double bestVal = autocorr (bestLag);

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
