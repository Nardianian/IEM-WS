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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "../Resources/AudioProcessorBase.h"
#include "WfsSpeakerProcessor.h"
#include "CustomUtilities.h"

/* Used in the background by IEM_JackAudio.h */
#define ProcessorClass WfsAudioProcessor

//==============================================================================
/**
    The Audio Processing class of the WaveFieldSynthesizer plugin.

    It handles all audio prcoessing functionalities needed for a JUCE PluginProcessor
    class. It also takes care of the input and output channel settings and of the
    WfsSpeakerArray.
*/
class WfsAudioProcessor  :  public AudioProcessorBase<IOTypes::AudioChannels<4>,
                                                      IOTypes::AudioChannels<48>>,
                            public Timer
{
  public:
    //==============================================================================
    /* These parameters are used in the background by IEM_JackAudio.h */
    constexpr static int numberOfInputChannels = 4;
    constexpr static int numberOfOutputChannels = 48;

    //========= JUCE Processor Template functions ==================================
    WfsAudioProcessor();
    ~WfsAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (AudioSampleBuffer&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Used for updates in case a value in the AudioProcessorValueTreeState changes. */
    void parameterChanged (const String &parameterID, float newValue) override;

    /** Can be used to implement custom behaviour for input and output buffers. */
    void updateBuffers() override;

    /** Manages state variables on timer call. */
    void timerCallback() override;

    /** Custom getter functions. */
    LoudspeakerArrangementType getLoudspeakerArrangementType();
    Array<WfsSpeakerProcessor*>& getWfsSpeakerArray();
    Range<float> getSourcePositionXRange();
    Range<float> getSourcePositionYRange();

    /** Creates a mono buffer out of an arbitrary-channel number buffer. */
    static AudioSampleBuffer makeMonoFromMultipleInputChannels(AudioSampleBuffer& buffer);

    /** Creates the AudioProcessorValueTreeState. */
    std::vector<std::unique_ptr<RangedAudioParameter>> createParameterLayout();

    /** State variables for managing the program state. */
    std::atomic<bool> scheduleLoudspeakerArrayUpdate;
    std::atomic<bool> scheduleLoudspeakerRedraw;
    std::atomic<bool> scheduleXYPadRedraw;
    std::atomic<bool> redrawLoudspeakers;
    std::atomic<bool> redrawXYPad;
    std::atomic<bool> updateLoudspeakerArray;

  private:
    //==============================================================================
    /** List of all audio parameters. Directly linked to AudioProcessorValueTreeState. */
    std::atomic<float>* inputChannelsSetting;
    std::atomic<float>* outputChannelsSetting;
    std::atomic<float>* loudspeakerArrangement;
    std::atomic<float>* nLoudspeakers;
    std::atomic<float>* loudspeakerArrayLength;
    std::atomic<float>* loudspeakerRadius;
    std::atomic<float>* rotation;
    std::atomic<float>* zoom;
    std::atomic<float>* sourcePositionX;
    std::atomic<float>* sourcePositionY;
    std::atomic<float>* listenerPositionX;
    std::atomic<float>* listenerPositionY;

    /** The FIR filter in the WFS signal chain and its coefficients. */
    dsp::FIR::Filter<float> firFilter;
    Array <float> coefArrayFIR = {0.354358, -0.013776, -0.013446, -0.013115, -0.012804, -0.01234, -0.012026, -0.011627,
                                  -0.011123, -0.010768, -0.010297, -0.009842, -0.009393, -0.008908, -0.008498, -0.007998, -0.007499, -0.007111,
                                  -0.006606, -0.006202, -0.005773, -0.005333, -0.004914, -0.004547, -0.004128, -0.003807, -0.003436, -0.003125,
                                  -0.002775, -0.002562, -0.002195, -0.002025, -0.001747, -0.001545, -0.001336, -0.001197, -0.000974, -0.000913,
                                  -0.000687, -0.000702, -0.000496, -0.00046,  -0.000394, -0.000361, -0.000248, -0.000301, -0.000224, -0.000196,
                                  -0.000179, -0.000249, -0.000181, -0.000211, -0.000240, -0.000178, -0.000333, -0.000183, -0.000344, -0.000257,
                                  -0.000376, -0.000292, -0.000368, -0.000397, -0.000338, -0.000468, -0.000352, -0.000469, -0.000382, -0.000469,
                                  -0.000446, -0.000428, -0.000457, -0.000377, -0.000542, -0.000173, -0.009305 };

    /** The loudspeaker array, each loudspeaker is a secondary sound source. */
    Array<WfsSpeakerProcessor*> wfsSpeakerArray;

    /** The circular buffer used as delay line for each loudspeaker signal. */
    juce::dsp::DelayLine<float> circularBuffer;

    /** Real number of loudspeakers used, limited by number of available channels. */
    int numChannelsUsed;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WfsAudioProcessor)
};
