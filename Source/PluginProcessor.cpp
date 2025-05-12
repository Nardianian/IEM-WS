/*
 ==============================================================================
 This file is based on the IEM plugin suite and might be included in the future.

 Authors: Lukas Ignaz Maier, email: maier.lukas1995@gmail.com
          Michael Reiter, email: michael.reiter94@gmail.com

 The IEM plug-in suite is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 The IEM plug-in suite is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this software.  If not, see <https://www.gnu.org/licenses/>.
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WfsAudioProcessor::WfsAudioProcessor() :
    AudioProcessorBase (
#ifndef JucePlugin_PreferredChannelConfigurations
        BusesProperties()
    #if ! JucePlugin_IsMidiEffect
        #if ! JucePlugin_IsSynth
            .withInput ("Input", AudioChannelSet::discreteChannels (10), true)
        #endif
            .withOutput ("Output", AudioChannelSet::discreteChannels (64), true)
    #endif
            ,
#endif
        createParameterLayout()),
    circularBuffer (88200),
    numChannelsUsed (2)
{
    /* Assign all pointers to AudioProcessorValueTreeState parameters. */
    inputChannelsSetting = parameters.getRawParameterValue ("inputChannelsSetting");
    outputChannelsSetting = parameters.getRawParameterValue ("outputChannelsSetting");
    loudspeakerArrangement = parameters.getRawParameterValue ("loudspeakerArrangement");
    nLoudspeakers = parameters.getRawParameterValue ("nLoudspeakers");
    loudspeakerArrayLength = parameters.getRawParameterValue ("loudspeakerArrayLength");
    loudspeakerRadius = parameters.getRawParameterValue ("loudspeakerRadius");
    rotation = parameters.getRawParameterValue ("rotation");
    zoom = parameters.getRawParameterValue ("zoom");
    sourcePositionX = parameters.getRawParameterValue ("sourcePositionX");
    sourcePositionY = parameters.getRawParameterValue ("sourcePositionY");
    listenerPositionX = parameters.getRawParameterValue ("listenerPositionX");
    listenerPositionY = parameters.getRawParameterValue ("listenerPositionY");

    /* Add listeners for each possible parameter change. */
    parameters.addParameterListener ("inputChannelsSetting", this);
    parameters.addParameterListener ("outputChannelsSetting", this);
    parameters.addParameterListener ("loudspeakerArrangement", this);
    parameters.addParameterListener ("nLoudspeakers", this);
    parameters.addParameterListener ("loudspeakerArrayLength", this);
    parameters.addParameterListener ("loudspeakerRadius", this);
    parameters.addParameterListener ("rotation", this);
    parameters.addParameterListener ("zoom", this);
    parameters.addParameterListener ("sourcePositionX", this);
    parameters.addParameterListener ("sourcePositionY", this);
    parameters.addParameterListener ("listenerPositionX", this);
    parameters.addParameterListener ("listenerPositionY", this);

    /* Initialize state variables. */
    scheduleLoudspeakerArrayUpdate = false;
    scheduleLoudspeakerRedraw = false;
    scheduleXYPadRedraw = false;
    redrawLoudspeakers = false;
    updateLoudspeakerArray = false;
    redrawXYPad = false;

    /* Update loudspeaker max. each 50ms after a change in GUI has happened. */
    startTimer (50);
}

WfsAudioProcessor::~WfsAudioProcessor()
{
    stopTimer();
}

int WfsAudioProcessor::getNumPrograms()
{
    /* >= 1, because some hosts don't cope very well if you tell them there are 0 programs. */
    return 1;
}

int WfsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void WfsAudioProcessor::setCurrentProgram (int index)
{
    ignoreUnused (index);
}

const String WfsAudioProcessor::getProgramName (int index)
{
    ignoreUnused (index);
    return {};
}

void WfsAudioProcessor::changeProgramName (int index, const String& newName)
{
    ignoreUnused (index, newName);
}

void WfsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const uint32 numChannels = 1;
    dsp::ProcessSpec spec ({ sampleRate, static_cast<uint32> (samplesPerBlock), numChannels });

    /* Initialize FIR filter. */
    juce::dsp::FIR::Coefficients<float>::Ptr coefficientsFIR =
        new juce::dsp::FIR::Coefficients<float> (coefArrayFIR.getRawDataPointer(),
                                                 coefArrayFIR.size());
    firFilter.coefficients = *coefficientsFIR;
    firFilter.prepare (spec);
    firFilter.reset();

    circularBuffer.prepare (spec);
    circularBuffer.reset();

    checkInputAndOutput (this,
                         static_cast<int> (*inputChannelsSetting),
                         static_cast<int> (*outputChannelsSetting),
                         true);
    ignoreUnused (sampleRate, samplesPerBlock);

    /* The number of LS is set according to GUI setting. T
     * There can not be more loudspeakers than #channels allowed (by DAW and GUI output settings). */
    if (static_cast<int> (*outputChannelsSetting) == 0)
        numChannelsUsed = jmin (static_cast<int> (*nLoudspeakers), getTotalNumOutputChannels());
    else
        numChannelsUsed =
            jmin (static_cast<int> (*nLoudspeakers), static_cast<int> (*outputChannelsSetting));

    /* Remove all loudspeakers and add them with updated settings. */
    wfsSpeakerArray.clear();
    for (int channel = 0; channel < numChannelsUsed; channel++)
    {
        wfsSpeakerArray.add (new WfsSpeakerProcessor (channel, numChannelsUsed));
        wfsSpeakerArray[channel]->setLoudspeakerCoordinates (*loudspeakerArrayLength,
                                                             *loudspeakerRadius,
                                                             MathConstants<float>::pi * (*rotation)
                                                                 / 180.0f,
                                                             getLoudspeakerArrangementType());
    }
}

void WfsAudioProcessor::releaseResources()
{
}

void WfsAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer&)
{
    checkInputAndOutput (this,
                         static_cast<int> (*inputChannelsSetting),
                         static_cast<int> (*outputChannelsSetting),
                         false);
    ScopedNoDenormals noDenormals;
    AudioSampleBuffer monoBuffer;

    if (updateLoudspeakerArray)
    {
        updateLoudspeakerArray = false;
        prepareToPlay (getSampleRate(), getBlockSize());
    }

    /* In case we have more outputs than inputs, clear excessive buffers. */
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    /* set class members (output pointer, nSamples, samplerate) */
    for (int channel = 0; channel < numChannelsUsed; channel++)
        wfsSpeakerArray[channel]->prepareToWrite (buffer.getWritePointer (channel),
                                                  buffer.getNumSamples(),
                                                  getSampleRate());

    /* convert input signal to mono (sum(channelValues)/nChannels)) */
    if (buffer.getNumChannels() > 1)
    {
        monoBuffer = makeMonoFromMultipleInputChannels (buffer);
        buffer.clear (1, 0, buffer.getNumSamples());
    }
    else
    {
        monoBuffer.makeCopyOf (buffer);
    }

    /* Process FIR filter. */
    auto audioBlock = juce::dsp::AudioBlock<float> (monoBuffer);
    auto context = juce::dsp::ProcessContextReplacing<float> (audioBlock);
    firFilter.process (context);

    /* Process delay. */
    const float* readPointerMonoSignal = monoBuffer.getReadPointer (0);
    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        circularBuffer.pushSample (0, readPointerMonoSignal[sample]);
        for (int channel = 0; channel < numChannelsUsed; ++channel)
        {
            wfsSpeakerArray[channel]->calculateDelayAndReadFromDelayLine (*sourcePositionX / *zoom,
                                                                          *sourcePositionY / *zoom,
                                                                          circularBuffer,
                                                                          sample);
        }
    }

    /* Process gain. */
    for (int channel = 0; channel < numChannelsUsed; ++channel)
        wfsSpeakerArray[channel]->calculateAndApplyGain (*sourcePositionX / *zoom,
                                                         *sourcePositionY / *zoom,
                                                         *listenerPositionX / *zoom,
                                                         *listenerPositionY / *zoom);
}

bool WfsAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* WfsAudioProcessor::createEditor()
{
    return new WfsAudioProcessorEditor (*this, parameters);
}

void WfsAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.copyState();

    auto oscConfig = state.getOrCreateChildWithName ("OSCConfig", nullptr);
    oscConfig.copyPropertiesFrom (oscParameterInterface.getConfig(), nullptr);

    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WfsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (ValueTree::fromXml (*xmlState));

            if (parameters.state.hasProperty ("OSCPort")) // legacy
            {
                oscParameterInterface.getOSCReceiver().connect (
                    parameters.state.getProperty ("OSCPort", var (-1)));
                parameters.state.removeProperty ("OSCPort", nullptr);
            }

            auto oscConfig = parameters.state.getChildWithName ("OSCConfig");

            if (oscConfig.isValid())
                oscParameterInterface.setConfig (oscConfig);
        }
}

void WfsAudioProcessor::parameterChanged (const String& parameterID, float newValue)
{
    DBG ("Parameter with ID " << parameterID << " has changed. New value: " << newValue);

    if (parameterID == "inputChannelsSetting" || parameterID == "outputChannelsSetting")
    {
        userChangedIOSettings = true;
        scheduleLoudspeakerArrayUpdate = true;
    }
    else if ((parameterID == "sourcePositionX") || (parameterID == "listenerPositionX")
             || (parameterID == "sourcePositionY") || (parameterID == "listenerPositionY"))
    {
        scheduleXYPadRedraw = true;
    }
    else if (parameterID == "zoom")
    {
        scheduleXYPadRedraw = true;
        scheduleLoudspeakerRedraw = true;
    }
    else if ((parameterID == "loudspeakerArrangement") || (parameterID == "nLoudspeakers")
             || (parameterID == "loudspeakerArrayLength") || (parameterID == "loudspeakerRadius")
             || (parameterID == "rotation"))
    {
        scheduleLoudspeakerArrayUpdate = true;
        scheduleLoudspeakerRedraw = true;
    }
}

void WfsAudioProcessor::updateBuffers()
{
    DBG ("IOHelper:  input size: " << input.getSize());
    DBG ("IOHelper: output size: " << output.getSize());
}

void WfsAudioProcessor::timerCallback()
{
    if (scheduleLoudspeakerArrayUpdate)
    {
        scheduleLoudspeakerArrayUpdate = false;
        scheduleLoudspeakerRedraw = false;

        updateLoudspeakerArray = true;
        redrawLoudspeakers = true;
    }
    else if (scheduleLoudspeakerRedraw)
    {
        scheduleLoudspeakerRedraw = false;

        redrawLoudspeakers = true;
    }
    else if (scheduleXYPadRedraw)
    {
        scheduleXYPadRedraw = false;

        redrawXYPad = true;
    }
}

LoudspeakerArrangementType WfsAudioProcessor::getLoudspeakerArrangementType()
{
    if (*loudspeakerArrangement > (static_cast<float> (LoudspeakerArrangementType::line) + 0.5f))
        return LoudspeakerArrangementType::circle;
    else
        return LoudspeakerArrangementType::line;
}

Array<WfsSpeakerProcessor*>& WfsAudioProcessor::getWfsSpeakerArray()
{
    return wfsSpeakerArray;
}

Range<float> WfsAudioProcessor::getSourcePositionXRange()
{
    return Range<float> (parameters.getParameterRange ("sourcePositionX").getRange().getStart(),
                         parameters.getParameterRange ("sourcePositionX").getRange().getEnd());
}

Range<float> WfsAudioProcessor::getSourcePositionYRange()
{
    return Range<float> (parameters.getParameterRange ("sourcePositionY").getRange().getStart(),
                         parameters.getParameterRange ("sourcePositionY").getRange().getEnd());
}

AudioSampleBuffer WfsAudioProcessor::makeMonoFromMultipleInputChannels (AudioSampleBuffer& buffer)
{
    AudioSampleBuffer newMonoBuffer;
    newMonoBuffer.setSize (1, buffer.getNumSamples());
    newMonoBuffer.clear();

    float* writePointerNewMonoBuffer = newMonoBuffer.getWritePointer (0);

    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        const float* readPointerInputBuffer = buffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            writePointerNewMonoBuffer[sample] += readPointerInputBuffer[sample];
        }
    }

    newMonoBuffer.applyGain (1.0f / buffer.getNumChannels());
    return newMonoBuffer;
}

std::vector<std::unique_ptr<RangedAudioParameter>> WfsAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "inputChannelsSetting",
        "Number of input channels ",
        "",
        NormalisableRange<float> (0.0f, 10.0f, 1.0f),
        0.0f,
        [] (float value) { return value < 0.5f ? "Auto" : String (value); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "outputChannelsSetting",
        "Number of output channels ",
        "",
        NormalisableRange<float> (0.0f, 48.0f, 1.0f),
        0.0f,
        [] (float value) { return value < 0.5f ? "Auto" : String (value); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "loudspeakerArrangement",
        "Loudspeaker Arrangement",
        "",
        NormalisableRange<float> (1.0f, 2.0f, 1.0f),
        static_cast<float> (LoudspeakerArrangementType::line),
        [] (float value) { return String (value); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "nLoudspeakers",
        "Number of loudspeakers",
        "",
        NormalisableRange<float> (1.0f, 64.0f, 1.0f),
        8.0f,
        [] (float value) { return String (value); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "loudspeakerArrayLength",
        "Loudspeaker array length",
        "m",
        NormalisableRange<float> (0.1f, 50.0f, 0.01f),
        5.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "loudspeakerRadius",
        "Loudspeaker radius",
        "m",
        NormalisableRange<float> (0.1f, 50.0f, 0.01f),
        5.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "rotation",
        "Loudspeaker rotation",
        "deg",
        NormalisableRange<float> (0.0f, 360.0f, 0.1f),
        0.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "zoom",
        "Zoom",
        "x",
        CustomUtilities::getNormalisableRangeExp (0.1f, 10.0f),
        1.0f,
        [] (float value) { return String (value, 2); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "sourcePositionX",
        "Source Position X",
        "m",
        NormalisableRange<float> (-10.0f, 10.0f, 0.01f),
        0.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "sourcePositionY",
        "Source Position Y",
        "m",
        NormalisableRange<float> (-7.5f, 7.5f, 0.01f),
        0.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "listenerPositionX",
        "Listener Position X",
        "m",
        NormalisableRange<float> (-10.0f, 10.0f, 0.01f),
        0.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    params.push_back (OSCParameterInterface::createParameterTheOldWay (
        "listenerPositionY",
        "Listener Position Y",
        "m",
        NormalisableRange<float> (-7.5f, 7.5f, 0.01f),
        0.0f,
        [] (float value) { return String (value, 1); },
        nullptr));

    return params;
}

/* This creates new instances of the plugin. */
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WfsAudioProcessor();
}