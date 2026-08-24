#include "PluginEditor.h"

//==============================================================================
KeyDetectorAudioProcessorEditor::KeyDetectorAudioProcessorEditor (KeyDetectorAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (spectrum);
    addAndMakeVisible (chroma);
    addAndMakeVisible (tuner);

    // --- Big detected-key readout ----------------------------------------------
    keyLabel.setJustificationType (juce::Justification::centred);
    keyLabel.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
    keyLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    keyLabel.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (keyLabel);

    detailLabel.setJustificationType (juce::Justification::centred);
    detailLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    detailLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    detailLabel.setText ("waiting for audio...", juce::dontSendNotification);
    addAndMakeVisible (detailLabel);

    // Host tempo readout (top-right of the header).
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xff2bd1a4));
    bpmLabel.setText ("-- BPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);

    // BPM octave multiplier (fixes half/double-time), next to the tempo readout.
    tempoMultBox.addItemList ({ "x0.5", "x1", "x2" }, 1);
    tempoMultBox.setTooltip ("Halve / double the detected BPM");
    addAndMakeVisible (tempoMultBox);
    tempoMultAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "tempoMult", tempoMultBox);

    // --- Controls ---------------------------------------------------------------
    smoothingSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    smoothingSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (smoothingSlider);
    smoothingAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "smoothing", smoothingSlider);

    smoothingLabel.setJustificationType (juce::Justification::centred);
    smoothingLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    smoothingLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (smoothingLabel);

    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "freeze", freezeButton);

    resetButton.onClick = [this] { processorRef.requestReset(); };
    addAndMakeVisible (resetButton);

    // Tuner mode selector (Auto / Pitch / Peak).
    tunerModeBox.addItemList ({ "Auto", "Pitch", "Peak" }, 1);
    addAndMakeVisible (tunerModeBox);
    tunerModeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "tunerMode", tunerModeBox);

    tunerModeLabel.setJustificationType (juce::Justification::centred);
    tunerModeLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tunerModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (tunerModeLabel);

    spectrumScratch.reserve ((size_t) KeyDetectorAudioProcessor::numBins);

    setSize (640, 500);
    startTimerHz (30);
}

KeyDetectorAudioProcessorEditor::~KeyDetectorAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void KeyDetectorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f14));

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    g.drawText ("Key Detector", 16, 10, 300, 24, juce::Justification::left, false);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("chroma / Krumhansl-Schmuckler key estimation  +  tuner  +  BPM",
                16, 32, 460, 18, juce::Justification::left, false);
}

void KeyDetectorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header row (top-right): BPM read-out + its ×½/×1/×2 octave selector.
    {
        auto header = juce::Rectangle<int> (area.getX(), 10, area.getWidth(), 26);
        tempoMultBox.setBounds (header.removeFromRight (64));
        header.removeFromRight (8);
        bpmLabel.setBounds (header.removeFromRight (140));
    }

    area.removeFromTop (40); // header

    // Bottom control strip.
    auto controls = area.removeFromBottom (72);
    {
        auto c = controls;
        auto knob = c.removeFromLeft (90);
        smoothingLabel.setBounds (knob.removeFromBottom (16));
        smoothingSlider.setBounds (knob);

        c.removeFromLeft (16);
        freezeButton.setBounds (c.removeFromLeft (80).withSizeKeepingCentre (80, 30));
        resetButton .setBounds (c.removeFromLeft (76).withSizeKeepingCentre (72, 30));

        // Tuner mode selector.
        c.removeFromLeft (10);
        auto tm = c.removeFromLeft (92);
        tunerModeLabel.setBounds (tm.removeFromBottom (16));
        tunerModeBox.setBounds (tm.withSizeKeepingCentre (92, 26));

        // Key readout occupies the rest of the strip.
        keyLabel.setBounds (c.removeFromTop (46));
        detailLabel.setBounds (c);
    }

    area.removeFromBottom (10);

    // Tuner strip.
    tuner.setBounds (area.removeFromBottom (68));
    area.removeFromBottom (10);

    // Chroma bars, spectrum fills the rest.
    chroma.setBounds (area.removeFromBottom (110));
    area.removeFromBottom (10);
    spectrum.setBounds (area);
}

//==============================================================================
void KeyDetectorAudioProcessorEditor::timerCallback()
{
    if (processorRef.copySpectrum (spectrumScratch))
        spectrum.setSpectrum (spectrumScratch, processorRef.getCurrentSampleRate());

    const auto ch = processorRef.getChromaSnapshot();
    chroma.setChroma (ch);

    const auto est = processorRef.getKeyEstimate();
    chroma.setTonic (est.pitchClass, est.isMinor);
    chroma.repaint();

    // Tempo estimated from the audio (shown only when a clear pulse is detected).
    const double bpm = processorRef.getBpm();
    bpmLabel.setText ((bpm > 0.0 && processorRef.getBpmConfidence() >= 0.25f)
                          ? juce::String (bpm, 1) + " BPM" : "-- BPM",
                      juce::dontSendNotification);

    // Tuner (falls back to the loudest spectral peak for percussion/inharmonic input).
    tuner.setReading (processorRef.getTunerFrequency(),
                      processorRef.getTunerClarity(),
                      processorRef.getTunerIsPitch());
    tuner.repaint();

    // Only show a key when the chroma is tonal enough.  Percussion / atonal input
    // gives a flat chroma that correlates poorly with any key profile, so show "--".
    float chromaSum = 0.0f;
    for (float v : ch)
        chromaSum += v;

    if (chromaSum > 1.0e-4f && est.correlation >= 0.6f)
    {
        keyLabel.setText (est.noteName(), juce::dontSendNotification);
        detailLabel.setText (
            "correlation " + juce::String (est.correlation, 2)
                + "    confidence " + juce::String (juce::roundToInt (est.confidence * 100.0f)) + "%",
            juce::dontSendNotification);
    }
    else
    {
        keyLabel.setText ("--", juce::dontSendNotification);
        detailLabel.setText (chromaSum > 1.0e-4f ? "no clear key" : "waiting for audio...",
                             juce::dontSendNotification);
    }
}
