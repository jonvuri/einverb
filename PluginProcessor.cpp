#include "PluginProcessor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
    addParameter(delayTime = new juce::AudioParameterFloat("delayTime",
                                                           "Delay Time",
                                                           (float)MIN_DELAY_TIME,
                                                           (float)MAX_DELAY_TIME,
                                                           (float)DEFAULT_DELAY_TIME));
    addParameter(delayGain = new juce::AudioParameterFloat("delayGain",
                                                           "Delay Gain",
                                                           (float)MIN_DELAY_GAIN,
                                                           (float)MAX_DELAY_GAIN,
                                                           (float)DEFAULT_DELAY_GAIN));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;

    const int numOutputChannels = juce::AudioProcessor::getTotalNumOutputChannels();

    // Extra samplesPerBlock is needed because we read/write in blocks, not continuous over samples
    const int delayBufferSize = static_cast<int>(MAX_DELAY_TIME * (sampleRate + samplesPerBlock) + 1);

    mDelayBuffer.setSize(numOutputChannels, delayBufferSize, false, false);
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor()
{
    return nullptr;
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    std::unique_ptr<juce::XmlElement> xml(new juce::XmlElement("Einverb"));

    xml->setAttribute("delayTime", (double)*delayTime);
    xml->setAttribute("delayGain", (double)*delayGain);

    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName("Einverb"))
        {
            *delayTime = (float)xmlState->getDoubleAttribute("delayTime", DEFAULT_DELAY_TIME);
            *delayGain = (float)xmlState->getDoubleAttribute("delayGain", DEFAULT_DELAY_GAIN);
        }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear buffers
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int bufferLength = buffer.getNumSamples();
    const int delayBufferLength = mDelayBuffer.getNumSamples();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float *bufferData = buffer.getReadPointer(channel);
        const float *delayBufferData = mDelayBuffer.getReadPointer(channel);

        fillDelayBuffer(channel, bufferLength, delayBufferLength, bufferData);
        getFromDelayBuffer(buffer, channel, bufferLength, delayBufferLength, delayBufferData);
    }

    // Increment delay buffer write position w/ wrapping
    mWritePosition = (mWritePosition + bufferLength) % delayBufferLength;
}

void AudioPluginAudioProcessor::fillDelayBuffer(const int channel, const int bufferLength, const int delayBufferLength, const float *bufferData)
{
    float gain = *delayGain;

    // Copy data from main buffer to delay buffer
    if (delayBufferLength > bufferLength + mWritePosition)
    {
        // We have enough room to copy buffer to delay buffer without wrapping

        mDelayBuffer.copyFromWithRamp(channel, mWritePosition, bufferData, bufferLength, gain, gain);
    }
    else
    {
        // We need to split buffer across end and wrapped start of delay buffer

        const int delayBufferRemaining = delayBufferLength - mWritePosition;

        mDelayBuffer.copyFromWithRamp(channel, mWritePosition, bufferData, delayBufferRemaining, gain, gain);
        mDelayBuffer.copyFromWithRamp(channel, 0, bufferData + delayBufferRemaining, bufferLength - delayBufferRemaining, 0.8F, 0.8F);
    }
}

void AudioPluginAudioProcessor::getFromDelayBuffer(juce::AudioBuffer<float> &buffer, const int channel, const int bufferLength, const int delayBufferLength, const float *delayBufferData)
{
    float time = *delayTime;

    const int delayOffset = static_cast<int>(mSampleRate * time);
    const int readPosition = (delayBufferLength + mWritePosition - delayOffset) % delayBufferLength;

    if (delayBufferLength > bufferLength + readPosition)
    {
        // We have enough room to add delay buffer to buffer without wrapping

        buffer.addFrom(channel, 0, delayBufferData + readPosition, bufferLength);
    }
    else
    {
        // We need to split the add from the delay buffer across end and wrapped start of buffer

        const int bufferRemaining = delayBufferLength - readPosition;

        buffer.addFrom(channel, 0, delayBufferData + readPosition, bufferRemaining);
        buffer.addFrom(channel, bufferRemaining, delayBufferData, bufferLength - bufferRemaining);
    }
}
