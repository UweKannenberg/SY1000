/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "Parameter.h"
#include <bitset>

//==============================================================================
SY1000AudioProcessor::SY1000AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] ***** START ***** ");

    // Add a parameter listener for all sliders 
    for (size_t i = 0; i < SY1000Param.size(); i++)
    {
        apvts.addParameterListener(juce::String(i), this);
    }

    // SY1000 Parameter Assingments
#include "ParameterAssignments.h"

    
 
 }

SY1000AudioProcessor::~SY1000AudioProcessor()
{
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] ***** END ***** ");
}

//==============================================================================
const juce::String SY1000AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SY1000AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SY1000AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SY1000AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SY1000AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SY1000AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SY1000AudioProcessor::getCurrentProgram()
{
    return 0;
}

void SY1000AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SY1000AudioProcessor::getProgramName (int index)
{
    return {};
}

void SY1000AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SY1000AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] prepareToPlay ");

    isNewMidiOutMessage = false;
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] activate SysEx sync....");
    sendSysEx("7F000001", 1, 1);


}

void SY1000AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SY1000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SY1000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    //for (int channel = 0; channel < totalNumInputChannels; ++channel)
    //{
        //auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    //}

    // Get the BPM info from Host
    playHead = this->getPlayHead();
    if (playHead != nullptr)
    {
        playHead->getCurrentPosition(currentPositionInfo);
        if (currentPositionInfo.bpm != myBPM)
        {
            // Host BPM value changed..
            myBPM = currentPositionInfo.bpm;
            if (isDebugMode) juce::Logger::writeToLog("[SY1000] Host BPM change :  " + juce::String(myBPM));
            // Send BPM to SY1000
            sendSysEx("1000123E", 4, (int)myBPM * 10);
            // Update the Master Effect BPM parameter
            updatePluginParameter("1000123E", 4, (int)myBPM * 10);
        }
    }

    // This plugin process no audio, only MIDI
    buffer.clear();

    // Check incoming MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isSysEx())
        {
            // check sysex header = "410000000069" SY1000 signature = ManufactorID + DeviceID + 4 Byte ModelID 
            juce::String sysex = juce::String::toHexString(message.getSysExData(), message.getSysExDataSize(), 0);
            if (sysex.startsWith("410000000069"))
            {
                juce::String incomingHex = "";
                int incomingValue = 0;
                int incomingDataBytes = 0;
                sysExInMessage = message.getDescription();
                switch (sysex.length())
                {
                case 26:
                    incomingDataBytes = 1;
                    incomingValue = message.getSysExData()[11];
                    break;
                case 28: // DataByteQty 2-4 -> Only the lowest 4bit of 7bit chars will be used by SY1000.
                    incomingDataBytes = 2;
                    incomingValue = 16 * message.getSysExData()[11] + message.getSysExData()[12];
                    break;
                case 30:
                    incomingDataBytes = 3;
                    incomingValue = 256 * message.getSysExData()[11] + 16 * message.getSysExData()[12] + message.getSysExData()[13];
                    break;
                case 32:
                    incomingDataBytes = 4;
                    incomingValue = 4096 * message.getSysExData()[11] + 256 * message.getSysExData()[12] + 16 * message.getSysExData()[13] + message.getSysExData()[14];
                    break;
                case 40: // for PatchLed ON OFF STATE
                    incomingDataBytes = 8;
                    incomingValue = 268435456 * message.getSysExData()[11] + 16777216 * message.getSysExData()[12] + 1048576 * message.getSysExData()[13] + 65536 * message.getSysExData()[14] + 4096 * message.getSysExData()[15] + 256 * message.getSysExData()[16] + 16 * message.getSysExData()[17] + message.getSysExData()[18];
                }
                if (incomingDataBytes > 0)
                {
                    incomingHex = sysex.substring(14, 22).toUpperCase();
                    if (isDebugMode) juce::Logger::writeToLog("[SY1000] SysEx IN  : " + message.getDescription() + " Hex = " + incomingHex + " DataBytes = " + juce::String(incomingDataBytes) + " Value = " + juce::String(incomingValue));
                    // New SysEx data -> Searches and sets the associated plugin parameter 
                    updatePluginParameter(incomingHex, incomingDataBytes, incomingValue);
                }
            }
        }
    }
    midiMessages.clear();

    // send MidiOut messages
    if (isNewMidiOutMessage)
    {
        isNewMidiOutMessage = false;
        //sysExOutMessage = midiOutMessage.getDescription();
        midiMessages.addEvent(midiOutMessage, 0);
    }
}

//==============================================================================
bool SY1000AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SY1000AudioProcessor::createEditor()
{
    //return new SY1000AudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SY1000AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] getStateInformation ");
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SY1000AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    if (isDebugMode) juce::Logger::writeToLog("[SY1000] setStateInformation ");
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void SY1000AudioProcessor::sendSysEx(juce::String hexAddress, int dataBytes, int value, bool forceSending)
{
    juce::uint8 SysEx[20] = { 0x41, 0x00, 0x00, 0x00, 0x00, 0x69, 0x12, 0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int remainder = 0;
    // patch HexAddress
    SysEx[7] = (juce::uint8)hexAddress.substring(0, 2).getHexValue32();
    SysEx[8] = (juce::uint8)hexAddress.substring(2, 4).getHexValue32();
    SysEx[9] = (juce::uint8)hexAddress.substring(4, 6).getHexValue32();
    SysEx[10] = (juce::uint8)hexAddress.substring(6, 8).getHexValue32();

    switch (dataBytes)
    {
    case 1:
        SysEx[11] = (juce::uint8)(value & 127);
        break;
    case 2: // DataByteQty 2-4 -> Only the lowest 4bit of 7bit chars will be used by SY1000.
        value = value & 255;
        SysEx[11] = (juce::uint8)(value / 16);
        remainder = value % 16;
        SysEx[12] = (juce::uint8)remainder;
        break;
    case 3:
        value = value & 4095;
        SysEx[11] = (juce::uint8)(value / 256);
        remainder = value % 256;
        SysEx[12] = (juce::uint8)(remainder / 16);
        remainder = remainder % 16;
        SysEx[13] = (juce::uint8)remainder;
        break;
    case 4:
        value = value & 65535;
        SysEx[11] = (juce::uint8)(value / 4096);
        remainder = value % 4096;
        SysEx[12] = (juce::uint8)(remainder / 256);
        remainder = remainder % 256;
        SysEx[13] = (juce::uint8)(remainder / 16);
        remainder = remainder % 16;
        SysEx[14] = (juce::uint8)remainder;
        break;
    case 8:
        //Value = Value & 4294967296;
        SysEx[11] = (juce::uint8)(value / 268435456);
        remainder = value % 268435456;
        SysEx[12] = (juce::uint8)(remainder / 16777216);
        remainder = remainder % 16777216;
        SysEx[13] = (juce::uint8)(remainder / 1048576);
        remainder = remainder % 1048576;
        SysEx[14] = (juce::uint8)(remainder / 65536);
        remainder = remainder % 65536;
        SysEx[15] = (juce::uint8)(remainder / 4096);
        remainder = remainder % 4096;
        SysEx[16] = (juce::uint8)(remainder / 256);
        remainder = remainder % 256;
        SysEx[17] = (juce::uint8)(remainder / 16);
        remainder = remainder % 16;
        SysEx[18] = (juce::uint8)remainder;
        break;
    }
    // Calculate Roland checksum of address and data bytes
    int checksum = 0;
    for (int i = 7; i <= 10 + dataBytes; i++)
    {
        checksum = checksum + SysEx[i];
        // always keep checksum under 128
        if (checksum > 128)
        {
            checksum = checksum - 128;
        }
    }
    // Substract checksum from 128 to get the final checksum
    checksum = 128 - checksum;
    // Patch checksum byte
    SysEx[(11 + dataBytes)] = (juce::uint8)checksum;

    // create SysEx message and set the isNewMidiMessage flag. SY1000AudioProcessor::processBlock will process the message
    midiOutMessage = juce::MidiMessage::createSysExMessage(SysEx, (12 + dataBytes));

    // Echo suppresson, don't send the same SysEx message that has been previously received. forceSending == true diable this behaviour
    if (forceSending || midiOutMessage.getDescription() != sysExInMessage)
    {
        isNewMidiOutMessage = true;
        if (isDebugMode) juce::Logger::writeToLog("[SY1000] SysEx OUT : " + midiOutMessage.getDescription() + " Hex = " + hexAddress + " DataBytes = " + juce::String(dataBytes) + " Value = " + juce::String(value));
    }
    else
    {
        if (isDebugMode) juce::Logger::writeToLog("[SY1000] SysEx OUT : ->  skipped (echo suppression)");
    }
}



// Searches and sets the associated plugin parameter value based on the received SysEx message data
void SY1000AudioProcessor::updatePluginParameter(juce::String hexAddress, int dataBytes, int newValue)
{
    parameterAttributes = SY1000Param.getParameterAttributes(hexAddress, dataBytes);
    if (parameterAttributes.isSingle)
    {
        if (SY1000Param.getParameterData(hexAddress, dataBytes, SY1000Parameter::ParameterType::SINGLE, parameterData))
        {
            updatePluginParameter(parameterData.parameterID, newValue, parameterData.parameterName);
        }
    }
    if (parameterAttributes.isDual)
    {
        if (SY1000Param.getParameterData(hexAddress, dataBytes, SY1000Parameter::ParameterType::DUALTIME, parameterData))
        {
            updatePluginParameter(parameterData.parameterID, newValue, parameterData.parameterName);
        }
    }
    if (parameterAttributes.isRegister)
    {
        if (SY1000Param.getParameterData(hexAddress, dataBytes, SY1000Parameter::ParameterType::REGISTER, parameterData))
        {
            if (hexAddress == "10000312")
            {
                registerA = newValue;
            }
            if (hexAddress == "1000031A")
            {
                registerB = newValue;
            }
            //updatePluginParameter(parameterData.parameterID, newValue, parameterData.parameterName);
            updatePluginRegisterbitParameter(hexAddress);
        }
    }
}


// Searches and sets the associated plugin parameter value based on the parameterID
void SY1000AudioProcessor::updatePluginParameter(juce::String parameterID, int newValue, juce::String parameterName)
{
    juce::RangedAudioParameter* rangedAudioParameter = apvts.getParameter(parameterID);
    if (rangedAudioParameter != nullptr)
    {
        // Update only if current Value is different from newValue
        if (newValue != rangedAudioParameter->convertFrom0to1(rangedAudioParameter->getValue()))
        {
            if (isDebugMode) juce::Logger::writeToLog("[SY1000] update parameter -> Value = " + juce::String(newValue) + " : ParameterName = " + parameterName);
            
            rangedAudioParameter->beginChangeGesture();
            rangedAudioParameter->setValueNotifyingHost(rangedAudioParameter->convertTo0to1((float)newValue));
            rangedAudioParameter->endChangeGesture();
           
        }
        
    }
}

// Searches and sets all register bit parameters 
void SY1000AudioProcessor::updatePluginRegisterbitParameter(juce::String hexAddress)
{
    int pow2 = 1;
    int BitValue = 0;
    juce::String hexAddress_Bit = "";

    int registerValue = 0;
    if (hexAddress == "10000312")
    {
        // Register A
        registerValue = registerA;
    }
    if (hexAddress == "1000031A")
    {
        // Register B
        registerValue = registerB;
    }
    for (int i = 0; i <= 31; i++)
    {
        char suffix[3];
        std::snprintf(suffix, sizeof(suffix), "%02d", i);
        hexAddress_Bit = hexAddress + "_" + suffix;
        BitValue = registerValue & pow2;
        if (BitValue > 0) BitValue = 1;
        if (SY1000Param.getParameterData(hexAddress_Bit, 1, SY1000Parameter::ParameterType::REGISTERBIT, parameterData))
        {
            updatePluginParameter(parameterData.parameterID, BitValue, parameterData.parameterName);
        }
        // next 2^î
        pow2 = pow2 * 2;
    }
}

void SY1000AudioProcessor::updatePresetParameter(juce::String parameterID, int newValue)
{
    juce::RangedAudioParameter* rangedAudioParameter = apvts.getParameter(parameterID);
    if (rangedAudioParameter != nullptr)
    {
        // Update only if current Value is different from newValue
        if (newValue != rangedAudioParameter->convertFrom0to1(rangedAudioParameter->getValue()))
        {
            if (isDebugMode) juce::Logger::writeToLog("[SY1000] update preset parameter -> Value = " + juce::String(newValue) + " : ID = " + parameterID);
            rangedAudioParameter->setValue(rangedAudioParameter->convertTo0to1((float)newValue));
        }

    }
}


juce::AudioProcessorValueTreeState::ParameterLayout SY1000AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
 
    for (int i = 0; i < SY1000Param.size(); i++)
    {
        if (SY1000Param.getParameterData(juce::String(i), parameterData))
        {
            if (parameterData.choices.size() == 0)
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterInt>(parameterData.parameterID, parameterData.parameterName, parameterData.minValue, parameterData.maxValue, parameterData.defaultValue));
            }
            else
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(parameterData.parameterID, parameterData.parameterName,parameterData.choices, parameterData.defaultValue));
            }
        }
    }
    return { parameterLayout.begin(), parameterLayout.end() };
}

void SY1000AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (SY1000Param.getParameterData(parameterID, parameterData))
    {
        if (isDebugMode) juce::Logger::writeToLog("[SY1000] parameter changed -> Value = " + juce::String(newValue) + " : ParameterName = " + parameterData.parameterName);
        if (parameterData.parameterType == SY1000Parameter::ParameterType::SINGLE)
        {
            if (parameterData.choices.size() == 0)
            {
                // normal parameter
                sendSysEx(parameterData.hexAddress, parameterData.dataBytes, (int)newValue);
            }
            else
            {
                // choices parameter
                sendSysEx(parameterData.hexAddress, parameterData.dataBytes, (int)newValue + parameterData.minValue);
            }

        }
        
        if (parameterData.parameterType == SY1000Parameter::ParameterType::DUALTIME)
        {
            // DUALTIME is the normal Time parameter. 
            sendSysEx(parameterData.hexAddress, parameterData.dataBytes, (int)newValue);
            // Update the corresponding virtual BPM parameter

            if (SY1000Param.getParameterData(parameterData.hexAddress, parameterData.dataBytes, SY1000Parameter::ParameterType::DUALBPM, parameterData_BPM))
            {
                int BPMValue = 0;
                // Update BPM only for newValues >= maxtimeValue
                if (newValue >= parameterData_BPM.maxtimeValue)
                {
                    BPMValue = (int)newValue - parameterData_BPM.maxtimeValue;
                }
                updatePluginParameter(parameterData_BPM.parameterID, BPMValue);
            }
        }

        if (parameterData.parameterType == SY1000Parameter::ParameterType::DUALBPM)
        {
            // DUALBPM is the virtual BPM parameter that controls only the upper part of the corresponding Time parameter
            //sendSysEx(parameterData.hexAddress, parameterData.dataBytes, newValue + parameterData.maxtimeValue);
            // Update the corresponding Time parameter if BPM newValue > 0 (TIME)
            SY1000Parameter::Data parameterData_Time;
            if (SY1000Param.getParameterData(parameterData.hexAddress, parameterData.dataBytes, SY1000Parameter::ParameterType::DUALTIME, parameterData_Time) && newValue > 0)
            {
                updatePluginParameter(parameterData_Time.parameterID, (int)newValue + parameterData.maxtimeValue);
            }
        }

        if (parameterData.parameterType == SY1000Parameter::ParameterType::REGISTER)
        {
            // The REGISTER Parameter is not activly used as a VST Value. 
        }

        if (parameterData.parameterType == SY1000Parameter::ParameterType::REGISTERBIT)
        {
            // REGISTERBIT is used for Pedal On/Off state. (See. SY-1000_MIDI_Implementation.pdf Page 76 Table 3+4)
            // Register A HexAddress = 10000312_XX -> XX = BIT Number (Table 3) 00-31
            // Register B HexAddress = 1000030B_YY -> YY = BIT Number (Table 4) 00-07
            juce::String hexAddress = parameterData.hexAddress.substring(0, 8);
            juce::String test = parameterData.hexAddress.substring(9);
            int bitPosition = parameterData.hexAddress.substring(9).getIntValue();
            int mask = 1 << bitPosition;
            //int registerValue = 0;
            int registerValue = 0;
            if (hexAddress == "10000312")
            {
                // Register A
                registerValue = registerA;
            }
            if (hexAddress == "1000031A")
            {
                // Register B
                registerValue = registerB;
            }
            if (newValue == 0)
            {
                // clearing bit at bitPosition
                registerValue = registerValue & ~mask;
            }
            else
            {
                // Setting bit at bitPosition
                registerValue = registerValue | mask;
            }
            // Update registerA or registerB with new registerValue
            if (hexAddress == "10000312")
            {
                // Register A
                registerA = registerValue;
                sendSysEx("10000312", 8, registerValue);
            }
            if (hexAddress == "1000031A")
            {
                // Register B
                registerB = registerValue;
                sendSysEx("1000031A", 8, registerValue);
            }
            
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SY1000AudioProcessor();
}
