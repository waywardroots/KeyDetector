#include "PitchDetector.h"

#include <algorithm>
#include <cmath>

//==============================================================================
void PitchDetector::prepare (double newSampleRate, int maxBufferSize)
{
    sampleRate = newSampleRate;

    // Integration window W = maxBufferSize/2, and tau ranges up to W.  The largest
    // tau we ever need corresponds to the lowest frequency.
    const int w = std::max (64, maxBufferSize / 2);
    maxTau = std::min (w, (int) std::ceil (sampleRate / minFreq)) + 1;

    diff.assign ((size_t) (maxTau + 2), 0.0f);
    cmnd.assign ((size_t) (maxTau + 2), 0.0f);
}

PitchDetector::Result PitchDetector::process (const float* x, int numSamples)
{
    Result result;

    const int w = numSamples / 2;              // integration window
    if (w < 64)
        return result;

    int tauMax = std::min (w, (int) std::ceil (sampleRate / minFreq));
    tauMax = std::min (tauMax, (int) diff.size() - 1);
    const int tauMin = std::max (2, (int) std::floor (sampleRate / maxFreq));
    if (tauMax <= tauMin + 2)
        return result;

    // Energy gate: don't try to find a pitch in silence.
    double energy = 0.0;
    for (int j = 0; j < w; ++j)
        energy += (double) x[j] * x[j];
    if (energy / w < 1.0e-7)
        return result;

    // 1) Difference function  d(tau) = sum_j (x[j] - x[j+tau])^2
    diff[0] = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < w; ++j)
        {
            const double d = (double) x[j] - (double) x[j + tau];
            sum += d * d;
        }
        diff[(size_t) tau] = (float) sum;
    }

    // 2) Cumulative mean normalised difference  d'(tau)
    cmnd[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += (double) diff[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? (float) ((double) diff[(size_t) tau] * tau / running)
                                           : 1.0f;
    }

    // 3) Absolute threshold: first dip below `threshold`, then descend to its local
    //    minimum.  Fall back to the global minimum if nothing crosses the threshold.
    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t)
    {
        if (cmnd[(size_t) t] < threshold)
        {
            while (t + 1 <= tauMax && cmnd[(size_t) (t + 1)] < cmnd[(size_t) t])
                ++t;
            tau = t;
            break;
        }
    }

    if (tau < 0)
    {
        int best = tauMin;
        for (int t = tauMin; t <= tauMax; ++t)
            if (cmnd[(size_t) t] < cmnd[(size_t) best])
                best = t;
        tau = best;
    }

    // 4) Parabolic interpolation around the chosen tau for sub-sample accuracy.
    double betterTau = tau;
    if (tau > tauMin && tau < tauMax)
    {
        const double a = cmnd[(size_t) (tau - 1)];
        const double b = cmnd[(size_t) tau];
        const double c = cmnd[(size_t) (tau + 1)];
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-12)
            betterTau = tau + 0.5 * (a - c) / denom;
    }

    const double freq = sampleRate / betterTau;
    if (freq < minFreq || freq > maxFreq)
        return result;

    result.frequency = (float) freq;
    result.clarity   = (float) std::clamp (1.0 - (double) cmnd[(size_t) tau], 0.0, 1.0);
    return result;
}

//==============================================================================
void PitchDetector::frequencyToNote (double hz, int& midiNote, int& pitchClass,
                                     int& octave, double& cents)
{
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    midiNote   = (int) std::lround (midi);
    cents      = (midi - midiNote) * 100.0;
    pitchClass = ((midiNote % 12) + 12) % 12;
    octave     = midiNote / 12 - 1;   // MIDI 69 (A) -> octave 4
}

std::string PitchDetector::noteName (int midiNote)
{
    static const char* names[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc  = ((midiNote % 12) + 12) % 12;
    const int oct = midiNote / 12 - 1;
    return std::string (names[pc]) + std::to_string (oct);
}

std::string PitchDetector::pitchClassName (int pitchClass)
{
    static const char* names[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pitchClass % 12) + 12) % 12];
}
