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

using namespace juce;

/** The enum needs to start at 1 because of cbLoudspeakerArrangement.addItem() */
enum class LoudspeakerArrangementType
{
    line = 1,
    circle
};

//==============================================================================
/**
    One WFS loudspeaker, acting as a secondary source in the wave field synthesis domain.

    Only part of the audio processing for each loudspeaker is handled in here. The rest is
    handled by the WfsAudioProcessor.
*/
class WfsSpeakerProcessor
{
public:
    WfsSpeakerProcessor (int channelToUse, int nLoudspeakersToUse);
    ~WfsSpeakerProcessor() = default;

    /** This function works the same way prepareToPlay() does. */
    void prepareToWrite (float* writePointerToUse, int nSamplesInBufferToUse, int sampleRateToUse);

    /** Set the loudspeaker coordinates using the GUI parameters. */
    Point<float>
        setLoudspeakerCoordinates (float lineLength,
                                   float radius,
                                   float rotation,
                                   LoudspeakerArrangementType loudspeakerArrangementTypeToUse);

    /** Update loudspeaker gain. */
    void calculateAndApplyGain (float sourcePosX,
                                float sourcePosY,
                                float listenerPosX,
                                float listenerPosY);

    /** Update the delay and read correspondingly from the delay line. */
    void calculateDelayAndReadFromDelayLine (float sourcePosX,
                                             float sourcePosY,
                                             juce::dsp::DelayLine<float>& delayLine,
                                             int sample);

    /** Getter functions. */
    float getXInMeters() const { return xCoordinate; }
    float getYInMeters() const { return yCoordinate; }

private:
    /** The unique loudspeaker properties. */
    float xCoordinate;
    float yCoordinate;
    int channel;
    int nLoudspeakers;

    /** Audio buffer variables. */
    float* writePointerOutputChannel;
    int nSamplesInBuffer;
    float sampleRate;

    /** State variables. */
    bool updateRingBufferReadPointer;

    /** Previous parameter values used for checking if something has changed. */
    float previousGain;
    float previousDelay;

    /** Numerical constants. */
    const float speedOfSound;
    const float epsilonFloat;
};