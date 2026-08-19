#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

//==============================================================================
/** A detailed FFT spectrum display.

    The raw magnitude spectrum is resampled into one column per horizontal pixel
    on a log-frequency axis, so every part of the range gets uniform detail (low
    frequencies are interpolated; high frequencies are peak-aggregated across the
    many bins that fall in each column).  Each column has fast-attack / slow-release
    smoothing plus a slowly-falling peak-hold trace, and the view has dB + frequency
    grids. */
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay() = default;

    /** Feed the latest linear magnitudes (bin k = k * sampleRate / (2*numBins)).
        This advances the per-column smoothing/peak state and repaints. */
    void setSpectrum (const std::vector<float>& magnitudes, double sampleRate);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildColumns();                 // resample mags -> per-pixel columns
    float freqToX (float hz) const noexcept;
    float dbToY   (float db) const noexcept;

    std::vector<float> mags;               // latest raw magnitudes
    double sampleRate = 44100.0;

    std::vector<float> colSmooth;          // per-pixel smoothed magnitude (linear)
    std::vector<float> colPeak;            // per-pixel peak-hold magnitude (linear)
    float refPeak = 1.0e-6f;               // running normalisation reference

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb   = -100.0f;
    static constexpr float maxDb   =  0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};

//==============================================================================
/** Draws the 12-bin chroma vector as labelled bars and highlights the tonic. */
class ChromaDisplay : public juce::Component
{
public:
    ChromaDisplay() = default;

    void setChroma (const std::array<float, 12>& newChroma) { chroma = newChroma; }
    void setTonic  (int pitchClass, bool minor) { tonicPc = pitchClass; isMinor = minor; }

    void paint (juce::Graphics& g) override;

private:
    std::array<float, 12> chroma {};
    int  tonicPc = -1;
    bool isMinor = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaDisplay)
};

//==============================================================================
/** A guitar/instrument-style tuner: shows the nearest note name and a cents needle
    for the current monophonic pitch.  Blank unless a clear single note is playing. */
class TunerDisplay : public juce::Component
{
public:
    TunerDisplay() = default;

    /** freqHz = detected fundamental (0 if none), clarity = 0..1 periodicity. */
    void setReading (float freqHz, float clarityIn) noexcept
    {
        frequency = freqHz;
        clarity   = clarityIn < 0.0f ? 0.0f : clarityIn;
    }

    void paint (juce::Graphics& g) override;

private:
    float frequency = 0.0f;
    float clarity   = 0.0f;

    static constexpr float showClarity = 0.6f; // hide the reading below this clarity

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerDisplay)
};
