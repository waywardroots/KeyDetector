#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"

//==============================================================================
/** GUI: a spectrum analyser, a chroma bar display, the detected key, plus the
    smoothing / freeze / reset controls.  A Timer polls the processor so the audio
    thread is never blocked by the GUI. */
class KeyDetectorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit KeyDetectorAudioProcessorEditor (KeyDetectorAudioProcessor&);
    ~KeyDetectorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    KeyDetectorAudioProcessor& processorRef;

    SpectrumDisplay spectrum;
    ChromaDisplay   chroma;

    juce::Label keyLabel;        // big "F# minor"
    juce::Label detailLabel;     // correlation / confidence

    juce::Slider      smoothingSlider;
    juce::ToggleButton freezeButton { "Freeze" };
    juce::TextButton   resetButton  { "Reset" };
    juce::Label        smoothingLabel { {}, "Smoothing" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> smoothingAttachment;
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    std::vector<float> spectrumScratch; // reused each timer tick

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyDetectorAudioProcessorEditor)
};
