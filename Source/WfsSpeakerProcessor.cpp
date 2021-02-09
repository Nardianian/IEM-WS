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

#include "WfsSpeakerProcessor.h"

WfsSpeakerProcessor::WfsSpeakerProcessor(int channelToUse, int nLoudspeakersToUse) :
    xCoordinate(0.0f), yCoordinate(0.0f), writePointerOutputChannel(nullptr),
    nSamplesInBuffer(512), updateRingBufferReadPointer(false),
    speedOfSound(343.0f), sampleRate(44100), previousGain(0.0f), previousDelay(0.0f),
    epsilonFloat(10.0f * std::numeric_limits<float>::epsilon())
{
    channel = channelToUse;
    nLoudspeakers = nLoudspeakersToUse;
    if (channel == nLoudspeakers - 1)
        updateRingBufferReadPointer = true;
    else
        updateRingBufferReadPointer = false;

}

Point<float> WfsSpeakerProcessor::setLoudspeakerCoordinates(float lineLength, float radius, float rotation, LoudspeakerArrangementType loudspeakerArrangementTypeToUse)
{
    Point<float> coordinatesInMeters;

    if (loudspeakerArrangementTypeToUse == LoudspeakerArrangementType::line)
    {
        /* Loudspeakers in line arrangement always sit on the x-axis. */
        float startingPointX = -lineLength / 2;
        float deltaX = lineLength / static_cast<float>(nLoudspeakers-1);

        xCoordinate = startingPointX + deltaX * static_cast<float>(channel);
        yCoordinate = 0.0f;
    }
    else if (loudspeakerArrangementTypeToUse == LoudspeakerArrangementType::circle)
    {
        float deltaPhi = 2 * MathConstants<float>::pi / static_cast<float>(nLoudspeakers);
        float phiZero = MathConstants<float>::pi / 2.0f + rotation;

        xCoordinate = cos(phiZero + static_cast<float>(channel) * deltaPhi) * radius;
        yCoordinate = sin(phiZero + static_cast<float>(channel) * deltaPhi) * radius;
    }

    if (nLoudspeakers > 1)
        coordinatesInMeters.addXY(xCoordinate, yCoordinate);
    else if (nLoudspeakers == 1)
        coordinatesInMeters.addXY(0.0f, 0.0f);
    else
        jassertfalse;

    return coordinatesInMeters;
}

void WfsSpeakerProcessor::calculateAndApplyGain(float sourcePosX, float sourcePosY, float listenerPosX, float listenerPosY)
{
    Point<float> listenerPosition(listenerPosX, listenerPosY);

    float gain;
    float distanceSourceToSpeaker = sqrt(pow(sourcePosX - xCoordinate, 2.0f) + pow(sourcePosY - yCoordinate, 2.0f)) + epsilonFloat;

    /* TODO implement a different formula for circle arrangement */
    LoudspeakerArrangementType loudspeakerArrangementTypeToUse = LoudspeakerArrangementType::line;

    if (loudspeakerArrangementTypeToUse == LoudspeakerArrangementType::line)
    {
        float deltaY = abs(listenerPosition.y) + abs(sourcePosY);

        gain = -sourcePosY * sqrt( abs(listenerPosition.y)/(2.0f* MathConstants<float>::pi*deltaY) ) / pow(distanceSourceToSpeaker, 1.5f);
    }
    else if (loudspeakerArrangementTypeToUse == LoudspeakerArrangementType::circle)
    {
        float distanceSourceToListener = sqrt(pow(sourcePosX - listenerPosition.x, 2.0f) + pow(sourcePosY - listenerPosition.y, 2.0f)) + epsilonFloat;
        gain = -sourcePosY * sqrt((distanceSourceToListener - distanceSourceToSpeaker) /  (2*MathConstants<float>::pi* distanceSourceToListener) / pow(distanceSourceToSpeaker, 1.5f)) ;
    }

    /* Set the max gain a speaker/channel can receive to 1. */
    gain = jmin(gain, 1.0f);

    /* Interpolate gain between sblocks. */
    float deltaGain = (gain - previousGain) / static_cast<float>(nSamplesInBuffer);
    for (int sample = 0; sample < nSamplesInBuffer; sample++)
    {
        gain = previousGain + deltaGain * static_cast<float>(sample + 1);
        writePointerOutputChannel[sample] = writePointerOutputChannel[sample] * gain;
    }

    /* Update previous value. */
    previousGain = gain;
}


void WfsSpeakerProcessor::prepareToWrite(float* writePointerToUse, int nSamplesInBufferToUse, int sampleRateToUse)
{
    writePointerOutputChannel = writePointerToUse;
    nSamplesInBuffer = nSamplesInBufferToUse;
    sampleRate = sampleRateToUse;
}

void WfsSpeakerProcessor::calculateDelayAndReadFromDelayLine(float sourcePosX, float sourcePosY, juce::dsp::DelayLine<float>& delayLine, int sampleIndex)
{
    float distanceSourceSpeaker = sqrt(pow(sourcePosX - xCoordinate, 2.0f) + pow(sourcePosY - yCoordinate, 2.0f));

    float delayInSeconds = distanceSourceSpeaker / speedOfSound;
    float delayInSamples = delayInSeconds * sampleRate;

    /* Calculate and limit the delay difference using the previous delay. */
    float deltaDelayInSamples = (delayInSamples - previousDelay) / static_cast<float>(nSamplesInBuffer - sampleIndex);
    deltaDelayInSamples = jlimit(-0.1f, 0.1f, deltaDelayInSamples);

    delayInSamples = previousDelay + deltaDelayInSamples;

    /* Write delayed sample into output buffer. */
    writePointerOutputChannel[sampleIndex] = delayLine.popSample(0, delayInSamples, updateRingBufferReadPointer);

    /* Update previous value. */
    previousDelay = delayInSamples;
}