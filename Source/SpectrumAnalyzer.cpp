#include "SpectrumAnalyzer.h"

#include <cmath>

//==============================================================================
void SpectrumDisplay::setSpectrum (const std::vector<float>& magnitudes, double newSampleRate)
{
    mags       = magnitudes;
    sampleRate = newSampleRate;
    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff141821));
    g.fillRoundedRectangle (bounds, 4.0f);

    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // ---- Frequency grid lines (log spaced) -------------------------------------
    auto freqToX = [this, w] (float f)
    {
        const float lo = std::log10 (minFreq);
        const float hi = std::log10 (maxFreq);
        return juce::jmap (std::log10 (juce::jlimit (minFreq, maxFreq, f)), lo, hi, 0.0f, w);
    };

    g.setFont (11.0f);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f })
    {
        const float x = freqToX (f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                    (int) x + 2, (int) h - 14, 34, 12, juce::Justification::left, false);
    }

    if (mags.size() < 4 || sampleRate <= 0.0)
        return;

    const int   numBins = (int) mags.size();
    const float binHz   = (float) (sampleRate / (2.0 * numBins));

    // Normalise against the current peak so the trace always fills the view.
    float peak = 1.0e-9f;
    for (float m : mags)
        peak = juce::jmax (peak, m);

    auto magToY = [h] (float magNorm)
    {
        // gain -> dB (kept local so this graphics component only needs gui_basics).
        const float db = magNorm > 1.0e-9f ? 20.0f * std::log10 (magNorm) : minDb;
        return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, h, 0.0f);
    };

    // ---- Build the spectrum path -----------------------------------------------
    juce::Path path;
    bool started = false;

    for (int k = 1; k < numBins; ++k)
    {
        const float f = k * binHz;
        if (f < minFreq || f > maxFreq)
            continue;

        const float x = freqToX (f);
        const float y = magToY (mags[(size_t) k] / peak);

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    if (started)
    {
        juce::Path filled = path;
        filled.lineTo (w, h);
        filled.lineTo (freqToX (minFreq), h);
        filled.closeSubPath();

        g.setColour (juce::Colour (0xff2bd1a4).withAlpha (0.18f));
        g.fillPath (filled);

        g.setColour (juce::Colour (0xff2bd1a4));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }
}

//==============================================================================
void ChromaDisplay::paint (juce::Graphics& g)
{
    static const char* names[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff141821));
    g.fillRoundedRectangle (bounds, 4.0f);

    const float labelH = 16.0f;
    const float w      = bounds.getWidth();
    const float h      = bounds.getHeight() - labelH;
    const float barW   = w / 12.0f;

    float peak = 1.0e-6f;
    for (float v : chroma)
        peak = juce::jmax (peak, v);

    for (int i = 0; i < 12; ++i)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, chroma[(size_t) i] / peak);
        const float barH = norm * (h - 4.0f);
        const float x    = i * barW;

        juce::Rectangle<float> bar (x + 2.0f, h - barH, barW - 4.0f, barH);

        const bool isTonic = (i == tonicPc);
        g.setColour (isTonic ? (isMinor ? juce::Colour (0xff6ea8ff)
                                        : juce::Colour (0xffffc857))
                             : juce::Colour (0xff2bd1a4).withAlpha (0.75f));
        g.fillRoundedRectangle (bar, 2.0f);

        g.setColour (isTonic ? juce::Colours::white : juce::Colours::white.withAlpha (0.55f));
        g.setFont (isTonic ? 13.0f : 12.0f);
        g.drawText (names[i], (int) x, (int) (h + 1.0f), (int) barW, (int) labelH,
                    juce::Justification::centred, false);
    }
}
