#include "PluginEditor.h"

//==============================================================================
KeyDetectorAudioProcessorEditor::KeyDetectorAudioProcessorEditor (KeyDetectorAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Everything lives inside `content`, which is laid out at the fixed design size
    // and scaled to fill the (resizable) editor.
    addAndMakeVisible (content);
    content.onPaint = [] (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0d0f14));

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawText ("Key Detector", 16, 10, 300, 24, juce::Justification::left, false);
    };

    content.addAndMakeVisible (spectrum);
    content.addAndMakeVisible (chroma);
    content.addAndMakeVisible (tuner);

    // --- Big detected-key readout ----------------------------------------------
    keyLabel.setJustificationType (juce::Justification::centred);
    keyLabel.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
    keyLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    keyLabel.setText ("--", juce::dontSendNotification);
    content.addAndMakeVisible (keyLabel);

    detailLabel.setJustificationType (juce::Justification::centred);
    detailLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    detailLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    detailLabel.setText ("waiting for audio...", juce::dontSendNotification);
    content.addAndMakeVisible (detailLabel);

    // Tempo readout (top-right of the header).
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xff2bd1a4));
    bpmLabel.setText ("-- BPM", juce::dontSendNotification);
    content.addAndMakeVisible (bpmLabel);

    // BPM octave multiplier (fixes half/double-time; also rescales a held BPM).
    tempoMultBox.addItemList ({ "x0.5", "x1", "x2" }, 1);
    tempoMultBox.setTooltip ("Halve / double the BPM (also rescales a held value)");
    content.addAndMakeVisible (tempoMultBox);
    tempoMultAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "tempoMult", tempoMultBox);

    // Tap tempo (shortcut: T): derive BPM from the spacing of button presses.
    tapButton.setTooltip ("Tap tempo (shortcut: T)");
    tapButton.addShortcut (juce::KeyPress ('t'));
    content.addAndMakeVisible (tapButton);
    tapButton.onClick = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastTapMs > 2000.0)   // long gap -> start a fresh tap session
            tapTimes.clear();
        lastTapMs = now;
        tapTimes.push_back (now);
        if (tapTimes.size() > 8)
            tapTimes.erase (tapTimes.begin());

        if (tapTimes.size() >= 2)
        {
            const double span = tapTimes.back() - tapTimes.front();
            const double avg  = span / (double) (tapTimes.size() - 1);
            if (avg > 0.0)
                tappedBpm = juce::jlimit (30.0, 300.0, 60000.0 / avg);
        }

        holdButton.setToggleState (false, juce::dontSendNotification);
    };

    // Hold (shortcut: H): freeze the BPM read-out; captures the current base value.
    holdButton.setClickingTogglesState (true);
    holdButton.setTooltip ("Freeze the BPM (shortcut: H). Then use -/+ and x0.5/x1/x2.");
    holdButton.addShortcut (juce::KeyPress ('h'));
    content.addAndMakeVisible (holdButton);
    holdButton.onClick = [this]
    {
        if (holdButton.getToggleState())
        {
            heldBaseBpm = currentLiveBase > 0.0 ? currentLiveBase
                        : (tappedBpm > 0.0 ? tappedBpm : 120.0);
            fineOffset  = 0.0;
        }
    };

    // Fine-tune -/+ (engages Hold if not already held).
    auto nudge = [this] (double delta)
    {
        if (! holdButton.getToggleState())
        {
            heldBaseBpm = currentLiveBase > 0.0 ? currentLiveBase
                        : (tappedBpm > 0.0 ? tappedBpm : 120.0);
            fineOffset  = 0.0;
            holdButton.setToggleState (true, juce::dontSendNotification);
        }
        fineOffset += delta;
    };
    bpmDownButton.setTooltip ("Fine-tune BPM down (-0.1)");
    bpmUpButton  .setTooltip ("Fine-tune BPM up (+0.1)");
    bpmDownButton.onClick = [nudge] { nudge (-0.1); };
    bpmUpButton  .onClick = [nudge] { nudge (+0.1); };
    content.addAndMakeVisible (bpmDownButton);
    content.addAndMakeVisible (bpmUpButton);

    setWantsKeyboardFocus (true); // so the T / H shortcuts are received

    // --- Controls ---------------------------------------------------------------
    smoothingSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    smoothingSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    content.addAndMakeVisible (smoothingSlider);
    smoothingAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "smoothing", smoothingSlider);

    smoothingLabel.setJustificationType (juce::Justification::centred);
    smoothingLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    smoothingLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    content.addAndMakeVisible (smoothingLabel);

    content.addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "freeze", freezeButton);

    resetButton.onClick = [this] { processorRef.requestReset(); };
    content.addAndMakeVisible (resetButton);

    // Tuner mode selector (Auto / Pitch / Peak).
    tunerModeBox.addItemList ({ "Auto", "Pitch", "Peak" }, 1);
    content.addAndMakeVisible (tunerModeBox);
    tunerModeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "tunerMode", tunerModeBox);

    tunerModeLabel.setJustificationType (juce::Justification::centred);
    tunerModeLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tunerModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    content.addAndMakeVisible (tunerModeLabel);

    spectrumScratch.reserve ((size_t) KeyDetectorAudioProcessor::numBins);

    // Make the editor resizable (corner grip + host edge-resize), keeping the
    // original aspect ratio so the scaled layout always fits.
    setResizable (true, true);
    setResizeLimits (designWidth * 3 / 4, designHeight * 3 / 4,  // 0.75x
                     designWidth * 2,     designHeight * 2);     // 2x
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (designWidth, designHeight);
    startTimerHz (30);
}

KeyDetectorAudioProcessorEditor::~KeyDetectorAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void KeyDetectorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f14)); // behind the scaled content
}

void KeyDetectorAudioProcessorEditor::resized()
{
    // Scale the fixed-size content to fill the current (resizable) editor.
    const float scale = (float) getWidth() / (float) designWidth;
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    layoutContent();
}

void KeyDetectorAudioProcessorEditor::layoutContent()
{
    auto area = juce::Rectangle<int> (0, 0, designWidth, designHeight).reduced (12);

    // Header row (top-right): [Tap][Hold]  [-] BPM [+]  [×½/×1/×2]
    {
        auto header = juce::Rectangle<int> (area.getX(), 10, area.getWidth(), 26);
        tempoMultBox.setBounds (header.removeFromRight (52));
        header.removeFromRight (6);
        bpmUpButton.setBounds (header.removeFromRight (24));
        bpmLabel.setBounds (header.removeFromRight (96));
        bpmDownButton.setBounds (header.removeFromRight (24));
        header.removeFromRight (8);
        holdButton.setBounds (header.removeFromRight (44));
        header.removeFromRight (4);
        tapButton.setBounds (header.removeFromRight (40));
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

        c.removeFromLeft (10);
        auto tm = c.removeFromLeft (92);
        tunerModeLabel.setBounds (tm.removeFromBottom (16));
        tunerModeBox.setBounds (tm.withSizeKeepingCentre (92, 26));

        keyLabel.setBounds (c.removeFromTop (46));
        detailLabel.setBounds (c);
    }

    area.removeFromBottom (10);
    tuner.setBounds (area.removeFromBottom (68));
    area.removeFromBottom (10);
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

    // Tempo read-out.  Colour: green = audio, blue = tapped, amber = held.
    // The ×½/×1/×2 multiplier and the -/+ fine-tune apply to whatever the base is
    // (live estimate/tap, or the value captured when Hold engaged).
    const double now      = juce::Time::getMillisecondCounterHiRes();
    const bool   tapValid = tappedBpm > 0.0 && (now - lastTapMs) < 5000.0;
    const double factor   = tempoMultFactor();

    const double audioBpm = processorRef.getBpm();
    const bool   audioOk  = audioBpm > 0.0 && processorRef.getBpmConfidence() >= 0.25f;
    currentLiveBase = tapValid ? tappedBpm : (audioOk ? audioBpm : 0.0);

    juce::String txt;
    juce::Colour bpmColour;
    if (holdButton.getToggleState())
    {
        const double v = juce::jlimit (20.0, 400.0, heldBaseBpm * factor + fineOffset);
        txt = juce::String (v, 1) + " BPM";
        bpmColour = juce::Colour (0xffffc857);            // amber = held
    }
    else if (currentLiveBase > 0.0)
    {
        txt = juce::String (currentLiveBase * factor, 1) + " BPM";
        bpmColour = tapValid ? juce::Colour (0xff6ea8ff)  // blue = tapped
                             : juce::Colour (0xff2bd1a4);  // green = audio
    }
    else
    {
        txt = "-- BPM";
        bpmColour = juce::Colours::white.withAlpha (0.4f);
    }
    bpmLabel.setText (txt, juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, bpmColour);

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

double KeyDetectorAudioProcessorEditor::tempoMultFactor() const
{
    switch (tempoMultBox.getSelectedId()) // ids: 1=x0.5, 2=x1, 3=x2
    {
        case 1:  return 0.5;
        case 3:  return 2.0;
        default: return 1.0;
    }
}
