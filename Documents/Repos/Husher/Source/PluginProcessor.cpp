#include "PluginProcessor.h"
#include "PluginEditor.h"

HusherAudioProcessor::HusherAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(sensitivityParam = new juce::AudioParameterFloat(
        "sensitivity", "Sensitivity", 
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
}

HusherAudioProcessor::~HusherAudioProcessor()
{
}

const juce::String HusherAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HusherAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool HusherAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool HusherAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double HusherAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HusherAudioProcessor::getNumPrograms()
{
    return 1;
}

int HusherAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HusherAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String HusherAudioProcessor::getProgramName (int index)
{
    return {};
}

void HusherAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void HusherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    detector.prepare(sampleRate, samplesPerBlock);
    recordingBuffer.prepare(sampleRate, 300); // 5 minutes max recording
}

void HusherAudioProcessor::releaseResources()
{
    detector.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HusherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void HusherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Add audio to recording buffer if recording
    if (recordingBuffer.isRecording())
    {
        recordingBuffer.addAudioData(buffer);
    }

    // Process audio for Hebrew ח detection
    auto confidence = detector.processAudio(buffer, getSensitivity());
    confidenceLevel.store(confidence);
    
    // Add detection to recording buffer if confidence is high enough
    if (confidence > getSensitivity() && recordingBuffer.isRecording())
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        recordingBuffer.addDetection(currentTime, confidence);
    }
    
    // Generate MIDI markers for detected ח sounds
    if (confidence > getSensitivity())
    {
        auto midiNote = juce::MidiMessage::noteOn(1, 60, 0.8f);
        midiNote.setTimeStamp(0);
        midiMessages.addEvent(midiNote, 0);
        
        auto midiNoteOff = juce::MidiMessage::noteOff(1, 60);
        midiNoteOff.setTimeStamp(buffer.getNumSamples() - 1);
        midiMessages.addEvent(midiNoteOff, buffer.getNumSamples() - 1);
    }
}

bool HusherAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HusherAudioProcessor::createEditor()
{
    return new HusherAudioProcessorEditor (*this);
}

void HusherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = juce::ValueTree("HusherState");
    state.setProperty("sensitivity", getSensitivity(), nullptr);
    
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void HusherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xmlState = getXmlFromBinary(data, sizeInBytes);
    
    if (xmlState.get() != nullptr)
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        if (state.isValid())
        {
            *sensitivityParam = state.getProperty("sensitivity", 0.5f);
        }
    }
}

void HusherAudioProcessor::startRecording()
{
    recordingBuffer.startRecording();
}

void HusherAudioProcessor::stopRecording()
{
    recordingBuffer.stopRecording();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HusherAudioProcessor();
}