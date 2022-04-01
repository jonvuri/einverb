#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    // Plugin parameters
    juce::AudioParameterFloat *delayTime; // in seconds

    constexpr static const double MIN_DELAY_TIME{0};
    constexpr static const double MAX_DELAY_TIME{2.0};
    constexpr static const double DEFAULT_DELAY_TIME{0.15};

    juce::AudioParameterFloat *delayGain;

    constexpr static const double MIN_DELAY_GAIN{0};
    constexpr static const double MAX_DELAY_GAIN{1.0};
    constexpr static const double DEFAULT_DELAY_GAIN{0.8};

    // Internal state
    juce::AudioBuffer<float> mDelayBuffer;
    int mWritePosition{0};
    double mSampleRate{0};

    // Internal methods
    void fillDelayBuffer(const int channel, const int bufferLength, const int delayBufferLength, const float *bufferData);
    void getFromDelayBuffer(juce::AudioBuffer<float> &buffer, const int channel, const int bufferLength, const int delayBufferLength, const float *delayBufferData);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
