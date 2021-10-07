/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Parameter.h"

//==============================================================================
/**
*/
class SY1000AudioProcessor  : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    SY1000AudioProcessor();
    ~SY1000AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
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
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;


    
    void sendSysEx(juce::String hexAddress, int dataBytes, int value, bool forceSending = false);
    void updatePluginParameter(juce::String hexAddress, int dataBytes, int newValue);
    void updatePluginParameter(juce::String parameterID, int newValue, juce::String parameterName = "");

    void updatePluginRegisterbitParameter(juce::String hexAddress);

    void updatePresetParameter(juce::String parameterID, int newValue);

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void parameterChanged(const juce::String& parameterID, float newValue);


    
    // SY1000 Parameter
    SY1000Parameter SY1000Param;
    SY1000Parameter::Data parameterData;
    SY1000Parameter::Data parameterData_BPM;
    SY1000Parameter::ParameterAttributes parameterAttributes;

    // AudioProcessorValueTreeState definitions
    juce::AudioProcessorValueTreeState apvts;

    // Host program BPM detection 
    juce::AudioPlayHead* playHead;
    juce::AudioPlayHead::CurrentPositionInfo currentPositionInfo;
    double myBPM = -1.0;


    juce::MidiMessage midiOutMessage;
    juce::String sysExInMessage = "";
    std::atomic<bool> isNewMidiOutMessage = false;
    std::atomic<int> registerA = 0;
    std::atomic<int> registerB = 0;


    // SY1000 Slider and SliderAttachment Definition
#include "ParameterDefinitions.h"

private:

    bool isDebugMode = true;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SY1000AudioProcessor)
};
