// Key Detector - a JamesandtheCat plug-in.  (c) 2026 AudioFuzz. All rights reserved.
// This software is licensed, not sold; use is governed by the End User License
// Agreement (see EULA.md). Unauthorised copying, distribution, or modification is
// prohibited.

#include "ChromaKeyDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

//==============================================================================
// Exact equal-tempered note frequencies (Hz), A4 = 440 Hz, exactly as supplied.
// Row = pitch class (0 = C … 11 = B), column = octave 0..8.
// We assign each FFT bin to the pitch class of the *nearest* of these notes
// (measured in log-frequency / cents), so the binning is anchored to these
// physical frequencies rather than only an abstract formula.
const double ChromaKeyDetector::referenceNoteFrequencies[ChromaKeyDetector::numPitchClasses][9] =
{
    //  oct0     oct1     oct2      oct3      oct4      oct5      oct6       oct7       oct8
    {  16.35,   32.70,   65.41,   130.81,   261.63,   523.25,  1046.50,   2093.00,   4186.01 }, // C
    {  17.32,   34.65,   69.30,   138.59,   277.18,   554.37,  1108.73,   2217.46,   4434.92 }, // C#/Db
    {  18.35,   36.71,   73.42,   146.83,   293.66,   587.33,  1174.66,   2349.32,   4698.63 }, // D
    {  19.45,   38.89,   77.78,   155.56,   311.13,   622.25,  1244.51,   2489.02,   4978.03 }, // D#/Eb
    {  20.60,   41.20,   82.41,   164.81,   329.63,   659.25,  1318.51,   2637.02,   5274.04 }, // E
    {  21.83,   43.65,   87.31,   174.61,   349.23,   698.46,  1396.91,   2793.83,   5587.65 }, // F
    {  23.12,   46.25,   92.50,   185.00,   369.99,   739.99,  1479.98,   2959.96,   5919.91 }, // F#/Gb
    {  24.50,   49.00,   98.00,   196.00,   392.00,   783.99,  1567.98,   3135.96,   6271.93 }, // G
    {  25.96,   51.91,  103.83,   207.65,   415.30,   830.61,  1661.22,   3322.44,   6644.88 }, // G#/Ab
    {  27.50,   55.00,  110.00,   220.00,   440.00,   880.00,  1760.00,   3520.00,   7040.00 }, // A
    {  29.14,   58.27,  116.54,   233.08,   466.16,   932.33,  1864.66,   3729.31,   7458.62 }, // A#/Bb
    {  30.87,   61.74,  123.47,   246.94,   493.88,   987.77,  1975.53,   3951.07,   7902.13 }  // B
};

//==============================================================================
// Krumhansl–Schmuckler / Krumhansl–Kessler (1982) key profiles.
// These are averaged "tonal hierarchy" ratings: how well each of the 12
// pitch classes fits a given key when the tonic sits at index 0.  We correlate
// the measured chroma against every rotation of these two templates (12 major +
// 12 minor) and pick the best match — a simple, well-established key finder.
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    constexpr double kMajorProfile[12] =
        { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };

    constexpr double kMinorProfile[12] =
        { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    const char* const kPitchClassNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Pearson correlation coefficient between two length-12 vectors.
    double pearson (const double* x, const double* y, int n)
    {
        double mx = 0.0, my = 0.0;
        for (int i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
        mx /= n; my /= n;

        double num = 0.0, dx2 = 0.0, dy2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double a = x[i] - mx;
            const double b = y[i] - my;
            num += a * b;
            dx2 += a * a;
            dy2 += b * b;
        }

        const double den = std::sqrt (dx2 * dy2);
        return den > 1.0e-12 ? num / den : 0.0;
    }
}

//==============================================================================
std::string ChromaKeyDetector::KeyEstimate::name() const
{
    return std::string (kPitchClassNames[pitchClass % 12]) + (isMinor ? " minor" : " major");
}

std::string ChromaKeyDetector::KeyEstimate::noteName() const
{
    return kPitchClassNames[pitchClass % 12];
}

//==============================================================================
void ChromaKeyDetector::prepare (double newSampleRate, int newFftSize)
{
    sampleRate = newSampleRate;
    fftSize    = newFftSize;
    reset();
}

void ChromaKeyDetector::nearestNote (double freqHz, int& pitchClass, double& cents)
{
    // Find the closest note on the A440 equal-tempered grid (in log-frequency),
    // then report the deviation from it in cents.
    int    bestPc  = 0;
    double bestRef = freqHz;
    double bestD   = 1.0e300;

    for (int pc = 0; pc < numPitchClasses; ++pc)
        for (int oct = 0; oct < 9; ++oct)
        {
            const double r = referenceNoteFrequencies[pc][oct];
            const double d = std::abs (std::log2 (freqHz / r));
            if (d < bestD) { bestD = d; bestPc = pc; bestRef = r; }
        }

    pitchClass = bestPc;
    cents      = 1200.0 * std::log2 (freqHz / bestRef);
}

void ChromaKeyDetector::reset()
{
    chroma.fill (0.0);
    stablePc  = -1;
    candPc    = -1;
    candCount = 0;
}

void ChromaKeyDetector::processSpectrum (const float* magnitudes, int numBins)
{
    if (frozen)
        return;

    numBins = std::min (numBins, fftSize / 2);
    if (numBins < 4)
        return;

    const double freqPerBin = sampleRate / (double) fftSize;

    // Peak-picking parameters.
    constexpr double kEps            = 1.0e-12;
    constexpr float  kPeakRelThresh  = 0.006f; // peaks must exceed 0.6% of the max bin

    // Global maximum used for the peak threshold (ignore silent frames).
    float maxMag = 0.0f;
    for (int k = 1; k < numBins; ++k)
        maxMag = std::max (maxMag, magnitudes[k]);
    if (maxMag < 1.0e-6f)
        return;

    const float threshold = maxMag * kPeakRelThresh;

    std::array<double, numPitchClasses> frame {};
    double total = 0.0;

    // Walk the spectrum, keeping only local maxima (spectral peaks).  This ignores
    // the broadband noise floor between partials, which is what previously diluted
    // the chroma and made the key hard to read.
    for (int k = 1; k < numBins - 1; ++k)
    {
        const float m = magnitudes[k];
        if (m < threshold)                       continue;
        if (! (m > magnitudes[k - 1] && m >= magnitudes[k + 1])) continue;

        // Parabolic (log-magnitude) interpolation -> sub-bin peak location, so the
        // frequency estimate isn't quantised to the FFT bin grid.
        const double a = std::log ((double) magnitudes[k - 1] + kEps);
        const double b = std::log ((double) m + kEps);
        const double c = std::log ((double) magnitudes[k + 1] + kEps);
        const double denom = a - 2.0 * b + c;
        const double delta = std::abs (denom) > kEps ? 0.5 * (a - c) / denom : 0.0;

        const double freq = ((double) k + delta) * freqPerBin;
        if (freq < fMin || freq > fMax)
            continue;

        const double energy = (double) m;

        // Assign the peak to the nearest note on the A440 grid.  (Peak-thresholding
        // above already removed the broadband noise floor; a peak that is more than
        // a quarter-tone from every note is treated as non-tonal and skipped.)
        int pc; double cents;
        nearestNote (freq, pc, cents);
        if (std::abs (cents) >= 50.0)
            continue;

        frame[(size_t) pc] += energy;
        total += energy;
    }

    if (total < 1.0e-9)
        return;

    // L1-normalise the frame so loudness changes don't bias the running chroma.
    for (auto& v : frame)
        v /= total;

    // Exponential moving average -> a stable chroma that settles over ~1 s.
    const double alpha = (double) smoothing;
    for (int i = 0; i < numPitchClasses; ++i)
        chroma[(size_t) i] = alpha * chroma[(size_t) i] + (1.0 - alpha) * frame[(size_t) i];
}

std::array<float, ChromaKeyDetector::numPitchClasses> ChromaKeyDetector::getChroma() const
{
    std::array<float, numPitchClasses> out {};

    double sum = 0.0;
    for (double v : chroma)
        sum += v;

    if (sum > 1.0e-12)
        for (int i = 0; i < numPitchClasses; ++i)
            out[(size_t) i] = (float) (chroma[(size_t) i] / sum);

    return out;
}

double ChromaKeyDetector::keyCorrelation (int key, bool minor) const
{
    double c[12];
    for (int i = 0; i < 12; ++i)
        c[i] = chroma[(size_t) i];

    const double* profile = minor ? kMinorProfile : kMajorProfile;
    double rotated[12];
    for (int j = 0; j < 12; ++j)
        rotated[j] = profile[(j - key + 12) % 12];

    return pearson (c, rotated, 12);
}

ChromaKeyDetector::KeyEstimate ChromaKeyDetector::estimateKey() const
{
    // --- Dominant-pitch method: the loudest pitch class is the key (major) --------
    if (keyMethod == KeyMethod::DominantPitch)
    {
        double total = 0.0;
        for (double v : chroma)
            total += v;

        int argmax = 0;
        for (int i = 1; i < 12; ++i)
            if (chroma[(size_t) i] > chroma[(size_t) argmax])
                argmax = i;

        double second = 0.0;
        for (int i = 0; i < 12; ++i)
            if (i != argmax && chroma[(size_t) i] > second)
                second = chroma[(size_t) i];

        const double mx = chroma[(size_t) argmax];

        KeyEstimate e;
        e.pitchClass  = argmax;
        e.isMinor     = false; // dominant-pitch keys are reported as major
        e.correlation = total > 1.0e-12 ? (float) (mx / total) : 0.0f;         // energy fraction
        e.confidence  = mx    > 1.0e-12 ? (float) std::clamp ((mx - second) / mx, 0.0, 1.0) : 0.0f;
        return e;
    }

    KeyEstimate best;
    double bestCorr   = -2.0;
    double secondCorr = -2.0;

    auto consider = [&] (double corr, int pc, bool minor)
    {
        if (corr > bestCorr)
        {
            secondCorr   = bestCorr;
            bestCorr     = corr;
            best.pitchClass = pc;
            best.isMinor    = minor;
        }
        else if (corr > secondCorr)
        {
            secondCorr = corr;
        }
    };

    // Correlate against every rotation of both templates.  Rotating the profile so
    // its tonic lands on pitch class `key` is the same as testing that key.
    for (int key = 0; key < 12; ++key)
    {
        consider (keyCorrelation (key, false), key, false);

        if (majorOnly)
            continue;   // GUI mode: never report a minor key

        consider (keyCorrelation (key, true), key, true);
    }

    best.correlation = (float) bestCorr;

    // Confidence = how far the winner beats the runner-up.  A margin of ~0.15 in
    // correlation is treated as fully confident; this is only used for the UI meter.
    const double margin = bestCorr - secondCorr;
    best.confidence = (float) std::clamp (margin / 0.15, 0.0, 1.0);

    return best;
}

ChromaKeyDetector::KeyEstimate ChromaKeyDetector::estimateStableKey (double frameSeconds)
{
    const KeyEstimate raw = estimateKey();

    // Hysteresis / debounce: a new key only takes over once it has been the
    // instantaneous winner for `keyHoldSeconds` of continuous frames.  Momentary
    // single-frame winners can't flip the reported key, so it stops jumping while
    // still following genuine key changes within ~1 s.
    const int holdFrames = std::max (1, (int) std::lround (keyHoldSeconds / std::max (1.0e-4, frameSeconds)));

    if (stablePc < 0)
    {
        // First estimate: adopt immediately.
        stablePc    = raw.pitchClass;
        stableMinor = raw.isMinor;
        candPc      = raw.pitchClass;
        candMinor   = raw.isMinor;
        candCount   = 0;
    }
    else if (raw.pitchClass == stablePc && raw.isMinor == stableMinor)
    {
        candCount = 0; // incumbent still winning; drop any pending challenger
        candPc    = stablePc;
        candMinor = stableMinor;
    }
    else
    {
        if (raw.pitchClass == candPc && raw.isMinor == candMinor)
            ++candCount;
        else { candPc = raw.pitchClass; candMinor = raw.isMinor; candCount = 1; }

        if (candCount >= holdFrames)
        {
            stablePc    = candPc;
            stableMinor = candMinor;
            candCount   = 0;
        }
    }

    KeyEstimate out;
    out.pitchClass  = stablePc;
    out.isMinor     = stableMinor;
    out.correlation = (float) keyCorrelation (stablePc, stableMinor);
    out.confidence  = raw.confidence;
    return out;
}
