#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

//==============================================================================
/** Draws an FFT magnitude spectrum on a log-frequency x-axis and a dB y-axis. */
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay() = default;

    /** Give it the latest linear magnitudes (bin k = k * sampleRate / (2*numBins)). */
    void setSpectrum (const std::vector<float>& magnitudes, double sampleRate);

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> mags;          // latest magnitudes
    double sampleRate = 44100.0;

    static constexpr float minFreq = 30.0f;    // Hz, low edge of the display
    static constexpr float maxFreq = 8000.0f;  // Hz, high edge
    static constexpr float minDb   = -90.0f;
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
