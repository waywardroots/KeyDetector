#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//==============================================================================
KeyDetectorAudioProcessor::KeyDetectorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    smoothingParam = apvts.getRawParameterValue ("smoothing");
    freezeParam    = apvts.getRawParameterValue ("freeze");

    publishedSpectrum.assign ((size_t) numBins, 0.0f);
}

KeyDetectorAudioProcessor::~KeyDetectorAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
KeyDetectorAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Smoothing = the EMA coefficient applied to the chroma each frame.
    // Higher = steadier reading, slower to react to key changes.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "smoothing", 1 }, "Smoothing",
        NormalisableRange<float> (0.0f, 0.99f, 0.001f), 0.85f));

    // Freeze = hold the current chroma / key (stop accumulating).
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "freeze", 1 }, "Freeze", false));

    return layout;
}

//==============================================================================
void KeyDetectorAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    detector.prepare (sampleRate, fftSize);
    detector.setMajorOnly (true);   // GUI shows major keys only (never minor)
    detector.setKeyMethod (ChromaKeyDetector::KeyMethod::DominantPitch); // loudest pitch class = key
    detector.setKeyHoldTime (0.7f); // a new key must persist ~0.7 s before it's shown

    pitchDetector.prepare (sampleRate, pitchWindow);
    pitchDetector.setFrequencyRange (40.0, 1500.0);
    smoothedFreq = 0.0f;

    fifo.fill (0.0f);
    fftData.fill (0.0f);
    writePos     = 0;
    hopCountdown = fftSize; // wait for one full window before the first analysis

    for (auto& c : publishedChroma)
        c.store (0.0f);

    const juce::SpinLock::ScopedLockType sl (spectrumLock);
    std::fill (publishedSpectrum.begin(), publishedSpectrum.end(), 0.0f);
}

void KeyDetectorAudioProcessor::releaseResources() {}

bool KeyDetectorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono or stereo, as long as input and output layouts match.
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void KeyDetectorAudioProcessor::pushSampleToFifo (float sample) noexcept
{
    // Circular write.  After the buffer is first filled we run an analysis every
    // hopSize samples (overlapping windows), which is what makes the display update
    // smoothly at high FFT sizes.
    fifo[(size_t) writePos] = sample;
    writePos = (writePos + 1) % fftSize;

    if (--hopCountdown <= 0)
    {
        analyseFrame();
        hopCountdown = hopSize;
    }
}

void KeyDetectorAudioProcessor::analyseFrame()
{
    // Reconstruct the last fftSize samples in time order (oldest first).  After the
    // most recent write, writePos points at the oldest sample in the ring.
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = fifo[(size_t) ((writePos + i) % fftSize)];

    // --- Tuner: monophonic pitch on the most recent, *un-windowed* samples --------
    {
        auto pr = pitchDetector.process (fftData.data() + (fftSize - pitchWindow), pitchWindow);
        publishedClarity.store (pr.clarity);

        if (pr.frequency > 0.0f && pr.clarity >= 0.5f)
        {
            // Geometric smoothing of the cents needle, but snap on note changes
            // (jumps larger than ~a third of a semitone).
            if (smoothedFreq <= 0.0f
                || std::abs (std::log2 (pr.frequency / smoothedFreq)) > 0.03)
                smoothedFreq = pr.frequency;
            else
                smoothedFreq = std::exp (0.6f * std::log (smoothedFreq)
                                       + 0.4f * std::log (pr.frequency));

            publishedFreq.store (smoothedFreq);
        }
    }

    if (resetRequested.exchange (false))
    {
        detector.reset();
        for (auto& c : publishedChroma)
            c.store (0.0f);
    }

    // Apply live parameter values.  The "smoothing" knob (0..1) is mapped to a
    // *time constant* rather than a raw per-frame coefficient, so the amount of
    // averaging doesn't change when the frame/overlap rate changes.
    const double hopSeconds = (double) hopSize / currentSampleRate;
    const float  smoothing01 = smoothingParam != nullptr ? smoothingParam->load() : 0.85f;
    const double tau = 0.1 + (double) smoothing01 * 3.9;              // 0.1 s .. ~4 s
    const double alpha = std::exp (-hopSeconds / tau);               // EMA coefficient
    detector.setSmoothing ((float) alpha);
    detector.setFrozen   (freezeParam != nullptr && freezeParam->load() > 0.5f);

    // 1) Window the frame (Hann) to reduce spectral leakage, then take the FFT.
    //    performFrequencyOnlyForwardTransform leaves |X[k]| in fftData[0..fftSize/2].
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    // 2) Update chroma + key estimate from the magnitude spectrum.
    detector.processSpectrum (fftData.data(), numBins);

    const auto est    = detector.estimateStableKey (hopSeconds);
    const auto chroma = detector.getChroma();

    // 3) Publish for the GUI (lock-free where possible).
    for (int i = 0; i < 12; ++i)
        publishedChroma[(size_t) i].store (chroma[(size_t) i]);

    publishedKeyPc.store    (est.pitchClass);
    publishedKeyMinor.store (est.isMinor);
    publishedCorr.store     (est.correlation);
    publishedConf.store     (est.confidence);

    // Spectrum is larger, so publish it under a try-lock (GUI copies it out).
    const juce::SpinLock::ScopedTryLockType sl (spectrumLock);
    if (sl.isLocked())
        std::copy (fftData.begin(), fftData.begin() + numBins, publishedSpectrum.begin());
}

void KeyDetectorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Mono-sum the input frame by frame and feed the analyser.  The audio itself is
    // passed through untouched (this is an analyser, not an effect).
    if (numChannels > 0)
    {
        const float* ch0 = buffer.getReadPointer (0);
        const float* ch1 = numChannels > 1 ? buffer.getReadPointer (1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            const float mono = ch1 != nullptr ? 0.5f * (ch0[n] + ch1[n]) : ch0[n];
            pushSampleToFifo (mono);
        }
    }
}

//==============================================================================
ChromaKeyDetector::KeyEstimate KeyDetectorAudioProcessor::getKeyEstimate() const
{
    ChromaKeyDetector::KeyEstimate e;
    e.pitchClass  = publishedKeyPc.load();
    e.isMinor     = publishedKeyMinor.load();
    e.correlation = publishedCorr.load();
    e.confidence  = publishedConf.load();
    return e;
}

std::array<float, 12> KeyDetectorAudioProcessor::getChromaSnapshot() const
{
    std::array<float, 12> out {};
    for (int i = 0; i < 12; ++i)
        out[(size_t) i] = publishedChroma[(size_t) i].load();
    return out;
}

bool KeyDetectorAudioProcessor::copySpectrum (std::vector<float>& dest) const
{
    const juce::SpinLock::ScopedTryLockType sl (spectrumLock);
    if (! sl.isLocked())
        return false;

    dest.resize (publishedSpectrum.size());
    std::copy (publishedSpectrum.begin(), publishedSpectrum.end(), dest.begin());
    return true;
}

//==============================================================================
void KeyDetectorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KeyDetectorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* KeyDetectorAudioProcessor::createEditor()
{
    return new KeyDetectorAudioProcessorEditor (*this);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyDetectorAudioProcessor();
}
