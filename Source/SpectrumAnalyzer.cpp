#include "SpectrumAnalyzer.h"

#include <cmath>

//==============================================================================
void SpectrumDisplay::setSpectrum (const std::vector<float>& magnitudes, double newSampleRate)
{
    mags       = magnitudes;
    sampleRate = newSampleRate;
    rebuildColumns();
}

void SpectrumDisplay::resized()
{
    // Force the per-pixel buffers to be rebuilt at the new width.
    colSmooth.clear();
    colPeak.clear();
    rebuildColumns();
}

float SpectrumDisplay::freqToX (float hz) const noexcept
{
    const float lo = std::log (minFreq), hi = std::log (maxFreq);
    const float f  = juce::jlimit (minFreq, maxFreq, hz);
    return (float) getWidth() * (std::log (f) - lo) / (hi - lo);
}

float SpectrumDisplay::dbToY (float db) const noexcept
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb,
                       (float) getHeight(), 0.0f);
}

void SpectrumDisplay::rebuildColumns()
{
    const int w = getWidth();
    if (w <= 0 || mags.size() < 4 || sampleRate <= 0.0)
        return;

    if ((int) colSmooth.size() != w)
    {
        colSmooth.assign ((size_t) w, 0.0f);
        colPeak  .assign ((size_t) w, 0.0f);
    }

    const int   numBins = (int) mags.size();
    const float binHz   = (float) (sampleRate / (2.0 * numBins));

    // Running normalisation reference: instant attack, slow release, so the trace
    // fills the view when there's signal and sinks to the floor on silence.
    float gpeak = 1.0e-9f;
    for (int k = 1; k < numBins; ++k)
    {
        const float f = k * binHz;
        if (f >= minFreq && f <= maxFreq)
            gpeak = juce::jmax (gpeak, mags[(size_t) k]);
    }
    refPeak = gpeak > refPeak ? gpeak : (refPeak * 0.9f + gpeak * 0.1f);
    refPeak = juce::jmax (refPeak, 1.0e-9f);

    const float logLo = std::log (minFreq);
    const float logHi = std::log (maxFreq);

    for (int x = 0; x < w; ++x)
    {
        const float fLo = std::exp (logLo + (logHi - logLo) * ((float) x)       / (float) w);
        const float fHi = std::exp (logLo + (logHi - logLo) * ((float) (x + 1)) / (float) w);

        const int kLo = (int) std::ceil  (fLo / binHz);
        const int kHi = (int) std::floor (fHi / binHz);

        float m = 0.0f;
        if (kHi >= kLo)
        {
            // Column spans one or more bins: take the peak (keeps narrow tones).
            for (int k = juce::jmax (1, kLo); k <= juce::jmin (numBins - 1, kHi); ++k)
                m = juce::jmax (m, mags[(size_t) k]);
        }
        else
        {
            // Column narrower than the bin spacing (low end): interpolate.
            const float fc = std::sqrt (fLo * fHi);
            const float bf = fc / binHz;
            int   k0 = juce::jlimit (1, numBins - 2, (int) bf);
            const float fr = juce::jlimit (0.0f, 1.0f, bf - (float) k0);
            m = mags[(size_t) k0] * (1.0f - fr) + mags[(size_t) (k0 + 1)] * fr;
        }

        // Fast attack, slower release for a readable, less jittery trace.
        auto& s = colSmooth[(size_t) x];
        s = (m >= s) ? m : (s * 0.7f + m * 0.3f);

        // Peak hold with a slow fall.
        auto& p = colPeak[(size_t) x];
        p = (m > p) ? m : (p * 0.985f);
    }

    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    g.setColour (juce::Colour (0xff141821));
    g.fillRoundedRectangle (bounds, 4.0f);

    // ---- dB grid (horizontal) ---------------------------------------------------
    g.setFont (10.0f);
    for (int db = 0; db >= -80; db -= 20)
    {
        const float y = dbToY ((float) db);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawText (juce::String (db), 2, (int) y + 1, 26, 12, juce::Justification::left, false);
    }

    // ---- frequency grid (vertical) ----------------------------------------------
    const float labelledFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : labelledFreqs)
    {
        const float x = freqToX (f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        const juce::String txt = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k"
                                              : juce::String ((int) f);
        g.drawText (txt, (int) x + 2, (int) h - 13, 34, 12, juce::Justification::left, false);
    }

    if (colSmooth.empty())
        return;

    auto magToY = [this] (float linear)
    {
        const float db = linear > 1.0e-9f ? 20.0f * std::log10 (linear / refPeak) : minDb;
        return dbToY (db);
    };

    // ---- filled spectrum --------------------------------------------------------
    juce::Path path;
    path.startNewSubPath (0.0f, magToY (colSmooth[0]));
    for (int x = 1; x < (int) colSmooth.size(); ++x)
        path.lineTo ((float) x, magToY (colSmooth[(size_t) x]));

    juce::Path filled = path;
    filled.lineTo (w, h);
    filled.lineTo (0.0f, h);
    filled.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2bd1a4).withAlpha (0.35f), 0, 0,
                                             juce::Colour (0xff2bd1a4).withAlpha (0.02f), 0, h, false));
    g.fillPath (filled);

    g.setColour (juce::Colour (0xff36e0b0));
    g.strokePath (path, juce::PathStrokeType (1.4f));

    // ---- peak-hold trace --------------------------------------------------------
    juce::Path peak;
    peak.startNewSubPath (0.0f, magToY (colPeak[0]));
    for (int x = 1; x < (int) colPeak.size(); ++x)
        peak.lineTo ((float) x, magToY (colPeak[(size_t) x]));

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.strokePath (peak, juce::PathStrokeType (1.0f));
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
