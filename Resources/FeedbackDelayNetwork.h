/*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Authors: Sebastian Grill, Daniel Rudrich
 Copyright (c) 2017 - Institute of Electronic Music and Acoustics (IEM)
 https://iem.at

 The IEM plug-in suite is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 The IEM plug-in suite is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this software.  If not, see <https://www.gnu.org/licenses/>.
 ==============================================================================
 */

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "FilterVisualizerHelper.h"
#include "WalshHadamard/fwht.h"
using namespace juce::dsp;
class FeedbackDelayNetwork : private ProcessorBase
{
    static constexpr int maxDelayLength = 30;

public:
    enum FdnSize
    {
        uninitialized = 0,
        ato = 1,
        femto = 2,
        pico = 4,
        nano = 8,
        tiny = 16,
        small = 32,
        big = 64,
        huge = 128,
        giant = 256
    };

    struct FilterParameter
    {
        float frequency = 1000.0f;
        float linearGain = 1.0f;
        float q = 0.7071f;
    };

    struct HPFilterParameter
    {
        int mode = 0;
        float frequency = 20.0f;
        float q = 0.7071f;
    };

    FeedbackDelayNetwork (FdnSize size = big)
    {
        updateFdnSize (size);
        setDelayLength (20);
        dryWet = 0.5f;
        primeNumbers = primeNumGen (5000);
        overallGain = 0.1f;
    }
    ~FeedbackDelayNetwork() {}

    void setDryWet (float newDryWet)
    {
        if (newDryWet < 0.f)
            params.newDryWet = 0.f;
        else if (newDryWet > 1.f)
            params.newDryWet = 1.f;
        else
            params.newDryWet = newDryWet;

        params.dryWetChanged = true;
    }

    void prepare (const juce::dsp::ProcessSpec& newSpec) override
    {
        spec = newSpec;
        isInitialized = true;

        indices = indexGen (fdnSize, delayLength);
        updateParameterSettings();
        updateGuiCoefficients();

        for (int ch = 0; ch < fdnSize; ++ch)
        {
            delayBufferVector[ch]->clear();
            lowShelfFilters[ch]->reset();
            highShelfFilters[ch]->reset();

            hpFilters[ch]->reset (0.0f);
            additionalHpFilters[ch]->reset (0.0f);
        }
    }

    void process (const juce::dsp::ProcessContextReplacing<float>& context) override
    {
        juce::ScopedNoDenormals noDenormals;

        // parameter change thread safety
        if (params.dryWetChanged)
        {
            dryWet = params.newDryWet;
            params.dryWetChanged = false;
        }

        if (params.filterParametersChanged)
        {
            lowShelfParameters = params.newLowShelfParams;
            highShelfParameters = params.newHighShelfParams;
            hpFilterParameters = params.newHPFilterParams;
            params.needParameterUpdate = true;
            params.filterParametersChanged = false;
        }

        if (params.networkSizeChanged)
        {
            params.needParameterUpdate = true;
            params.networkSizeChanged = false;
            updateFdnSize (params.newNetworkSize);
        }

        if (params.delayLengthChanged)
        {
            delayLength = params.newDelayLength;
            indices = indexGen (fdnSize, delayLength);
            params.needParameterUpdate = true;
            params.delayLengthChanged = false;
        }

        if (params.overallGainChanged)
        {
            overallGain = params.newOverallGain;
            params.overallGainChanged = false;
            params.needParameterUpdate = true;
        }

        if (params.needParameterUpdate)
            updateParameterSettings();
        params.needParameterUpdate = false;

        juce::dsp::AudioBlock<float>& buffer = context.getOutputBlock();

        const int nChannels = static_cast<int> (buffer.getNumChannels());
        const int numSamples = static_cast<int> (buffer.getNumSamples());

        //TODO andere belegung?

        // if more channels than network order, mix pairs of high order channels
        // until order == number of channels
        //        if (nChannels > fdnSize)
        //        {
        //            int diff = nChannels - fdnSize;
        //            int start_index = nChannels - diff * 2;
        //
        //            for (int num = 0; num < diff; ++num)
        //            {
        //                int idx = start_index + num;
        //
        //                float* writeIn = buffer.getChannelPointer (idx);
        //                const float* writeOut1 = buffer.getChannelPointer (idx + num);
        //                const float* writeOut2 = buffer.getChannelPointer (idx + num + 1);
        //
        //                for (int i = 0; i < numSamples; ++i)
        //                {
        //                    writeIn[i] = (writeOut1[i] + writeOut2[i]) / sqrt(2.f);
        //                }
        //            }
        //        }

        float dryGain = 1.0f - dryWet;

        for (int i = 0; i < numSamples; ++i)
        {
            // apply delay to each channel for one time sample
            for (int channel = 0; channel < fdnSize; ++channel)
            {
                const int idx = std::min (channel, nChannels - 1);
                float* const channelData = buffer.getChannelPointer (idx);
                float* const delayData = delayBufferVector[channel]->getWritePointer (0);

                int delayPos = delayPositionVector[channel];

                const float inSample = channelData[i];
                if (! freeze)
                {
                    // data exchange between IO buffer and delay buffer

                    if (channel < nChannels)
                        delayData[delayPos] += inSample;
                }

                if (! freeze)
                {
                    // Apply highpass filter
                    if (hpFilterParameters.mode != 0)
                        delayData[delayPos] =
                            hpFilters[channel]->processSample (delayData[delayPos]);

                    if (hpFilterParameters.mode == 3)
                        delayData[delayPos] =
                            additionalHpFilters[channel]->processSample (delayData[delayPos]);
                    // apply shelving filters
                    delayData[delayPos] =
                        highShelfFilters[channel]->processSingleSampleRaw (delayData[delayPos]);
                    delayData[delayPos] =
                        lowShelfFilters[channel]->processSingleSampleRaw (delayData[delayPos]);
                }

                if (channel < nChannels)
                {
                    channelData[i] = delayData[delayPos] * dryWet;
                    channelData[i] += inSample * dryGain;
                }
                if (! freeze)
                    transferVector.set (channel, delayData[delayPos] * feedbackGainVector[channel]);
                else
                    transferVector.set (channel, delayData[delayPos]);
            }

            // perform fast walsh hadamard transform
            fwht (transferVector.getRawDataPointer(), transferVector.size());

            // write back into delay buffer
            // increment the delay buffer pointer
            for (int channel = 0; channel < fdnSize; ++channel)
            {
                float* const delayData =
                    delayBufferVector[channel]->getWritePointer (0); // the buffer is single channel

                int delayPos = delayPositionVector[channel];

                delayData[delayPos] = transferVector[channel];

                if (++delayPos >= delayBufferVector[channel]->getNumSamples())
                    delayPos = 0;

                delayPositionVector.set (channel, delayPos);
            }
        }
        // if more channels than network order, mix pairs of high order channels
        // until order == number of channels
        //        if (nChannels > fdnSize)
        //        {
        //            int diff = nChannels - fdnSize;
        //            int start_index = nChannels - diff * 2;
        //
        //            for (int num = diff - 1; num < 0; --num)
        //            {
        //                int idx = start_index + num;
        //                float *const writeOut = buffer.getChannelPointer (idx);
        //                float *const writeIn1 = buffer.getChannelPointer (idx + num);
        //                float *const writeIn2 = buffer.getChannelPointer (idx + num + 1);
        //
        //                for (int i = 0; i < numSamples; ++i)
        //                {
        //                    writeIn1[i] = writeOut[i] / sqrt(2.f);
        //                    writeIn2[i] = writeIn1[i];
        //                }
        //            }
        //        }
    }

    void setDelayLength (int newDelayLength)
    {
        params.newDelayLength = juce::jmin (newDelayLength, maxDelayLength);
        params.delayLengthChanged = true;
    }

    void reset() override {}
    void setFilterParameter (FilterParameter lowShelf,
                             FilterParameter highShelf,
                             HPFilterParameter hp)
    {
        params.newLowShelfParams = lowShelf;
        params.newHighShelfParams = highShelf;
        params.newHPFilterParams = hp;
        params.filterParametersChanged = true;
    }

    void setT60InSeconds (float reverbTime)
    {
        double temp;
        double t = double (reverbTime);
        temp = -60.0 / (20.0 * t);
        params.newOverallGain = pow (10.0, temp);
        params.overallGainChanged = true;
    }

    void setOverallGainPerSecond (float gainPerSecond)
    {
        params.newOverallGain = gainPerSecond;
        params.overallGainChanged = true;
    }

    juce::dsp::IIR::Coefficients<double>::Ptr getCoefficientsForGui (const int filterIndex) const
    {
        return guiCoefficients[filterIndex];
    }

    void updateGuiCoefficients()
    {
        // Highpass coefficients
        switch (hpFilterParameters.mode)
        {
            case 0:
                guiCoefficients[0] =
                    IIR::Coefficients<double>::makeAllPass (spec.sampleRate, 20.0f);
                break;
            case 1:
                guiCoefficients[0] = IIR::Coefficients<double>::makeFirstOrderHighPass (
                    spec.sampleRate,
                    juce::jmin (0.5 * spec.sampleRate,
                                static_cast<double> (hpFilterParameters.frequency)));
                break;
            case 2:
                guiCoefficients[0] = IIR::Coefficients<double>::makeHighPass (
                    spec.sampleRate,
                    juce::jmin (0.5 * spec.sampleRate,
                                static_cast<double> (hpFilterParameters.frequency)),
                    hpFilterParameters.q);
                break;
            case 3:
            {
                auto coeffs = IIR::Coefficients<double>::makeHighPass (
                    spec.sampleRate,
                    juce::jmin (0.5 * spec.sampleRate,
                                static_cast<double> (hpFilterParameters.frequency)));
                coeffs->coefficients =
                    FilterVisualizerHelper<double>::cascadeSecondOrderCoefficients (
                        coeffs->coefficients,
                        coeffs->coefficients);

                guiCoefficients[0] = coeffs;
                break;
            }

            default:
                guiCoefficients[0] =
                    IIR::Coefficients<double>::makeAllPass (spec.sampleRate, 20.0f);
                break;
        }

        // Lowshelf
        guiCoefficients[1] = IIR::Coefficients<double>::makeLowShelf (
            spec.sampleRate,
            juce::jmin (0.5 * spec.sampleRate, static_cast<double> (lowShelfParameters.frequency)),
            lowShelfParameters.q,
            lowShelfParameters.linearGain);

        // Highshelf
        guiCoefficients[2] = IIR::Coefficients<double>::makeHighShelf (
            spec.sampleRate,
            juce::jmin (0.5 * spec.sampleRate, static_cast<double> (highShelfParameters.frequency)),
            highShelfParameters.q,
            highShelfParameters.linearGain);

        repaintFV = true;
    }

    void getT60ForFrequencyArray (double* frequencies, double* t60Data, size_t numSamples)
    {
        std::vector<double> temp;
        temp.resize (numSamples);

        juce::dsp::IIR::Coefficients<double> coefficients;
        updateGuiCoefficients();

        for (int i = 0; i < 3; ++i)
        {
            coefficients = *getCoefficientsForGui (i);
            coefficients.getMagnitudeForFrequencyArray (frequencies,
                                                        &temp[0],
                                                        numSamples,
                                                        spec.sampleRate);
        }

        juce::FloatVectorOperations::multiply (&temp[0], t60Data, static_cast<int> (numSamples));
        juce::FloatVectorOperations::multiply (&temp[0],
                                               overallGain,
                                               static_cast<int> (numSamples));

        for (int i = 0; i < numSamples; ++i)
        {
            t60Data[i] = -3.0 / log10 (temp[i]);
        }
    }

    void setFreeze (bool shouldFreeze)
    {
        freeze = shouldFreeze;
        if (freeze)
            DBG ("freeze is true");
    }

    void setFdnSize (FdnSize size)
    {
        if (fdnSize != size)
        {
            params.newNetworkSize = size;
            params.networkSizeChanged = true;
        }
    }

    const FdnSize getFdnSize() { return params.newNetworkSize; }
    std::atomic<bool> repaintFV = true;

private:
    //==============================================================================
    juce::dsp::ProcessSpec spec = { 48000.0, 0, 0 };

    juce::OwnedArray<juce::AudioBuffer<float>> delayBufferVector;
    juce::OwnedArray<juce::IIRFilter> highShelfFilters;
    juce::OwnedArray<juce::IIRFilter> lowShelfFilters;

    juce::dsp::IIR::Coefficients<float>::Ptr hpCoefficients =
        juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0, 20.0f);

    juce::dsp::IIR::Coefficients<float>::Ptr additionalHpCoefficients =
        juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0, 20.0f);

    juce::OwnedArray<juce::dsp::IIR::Filter<float>> hpFilters;
    juce::OwnedArray<juce::dsp::IIR::Filter<float>> additionalHpFilters;

    juce::dsp::IIR::Coefficients<double>::Ptr guiCoefficients[3];

    juce::Array<int> delayPositionVector;
    juce::Array<float> feedbackGainVector;
    juce::Array<float> transferVector;

    std::vector<int> primeNumbers;
    std::vector<int> indices;

    FilterParameter lowShelfParameters, highShelfParameters;
    HPFilterParameter hpFilterParameters;
    float dryWet;
    float delayLength = 20;
    float overallGain;

    bool freeze = false;
    bool isInitialized = false;
    FdnSize fdnSize = uninitialized;

    struct UpdateStruct
    {
        bool dryWetChanged = false;
        float newDryWet = 0;

        bool filterParametersChanged = false;
        FilterParameter newLowShelfParams;
        FilterParameter newHighShelfParams;
        HPFilterParameter newHPFilterParams;

        bool delayLengthChanged = false;
        int newDelayLength = 20;

        bool networkSizeChanged = false;
        FdnSize newNetworkSize = FdnSize::big;

        bool overallGainChanged = false;
        float newOverallGain = 0.5;

        bool needParameterUpdate = false;
    };

    UpdateStruct params;

    inline int delayLengthConversion (int channel)
    {
        // we divide by 10 to get better range for room size setting
        float delayLenMillisec = primeNumbers[indices[channel]] / 10.f;
        return int (delayLenMillisec / 1000.f * spec.sampleRate); //convert to samples
    }

    inline float channelGainConversion (int channel, float gain)
    {
        int delayLenSamples = delayLengthConversion (channel);

        double length = double (delayLenSamples) / double (spec.sampleRate);
        return pow (gain, length);
    }

    std::vector<int> indexGen (FdnSize nChannels, int delayLength)
    {
        const int firstIncrement = delayLength / 10;
        const int finalIncrement = delayLength;

        std::vector<int> indices;

        if (firstIncrement < 1)
            indices.push_back (1.f);
        else
            indices.push_back (firstIncrement);

        float increment;
        int index;

        for (int i = 1; i < nChannels; i++)
        {
            increment =
                firstIncrement + abs (finalIncrement - firstIncrement) / float (nChannels) * i;

            if (increment < 1)
                increment = 1.f;

            index = int (round (indices[i - 1] + increment));
            indices.push_back (index);
        }
        return indices;
    }

    std::vector<int> primeNumGen (int count)
    {
        std::vector<int> series;

        int range = 3;
        while (series.size() < count)
        {
            bool is_prime = true;
            for (int i = 2; i < range; i++)
            {
                if (range % i == 0)
                {
                    is_prime = false;
                    break;
                }
            }

            if (is_prime)
                series.push_back (range);

            range += 2;
        }
        return series;
    }

    //------------------------------------------------------------------------------
    inline void updateParameterSettings()
    {
        indices = indexGen (fdnSize, delayLength);

        for (int channel = 0; channel < fdnSize; ++channel)
        {
            // update multichannel delay parameters
            int delayLenSamples = delayLengthConversion (channel);
            delayBufferVector[channel]->setSize (1, delayLenSamples, true, true, true);
            if (delayPositionVector[channel] >= delayBufferVector[channel]->getNumSamples())
                delayPositionVector.set (channel, 0);
        }
        updateFeedBackGainVector();
        updateFilterCoefficients();
    }

    void updateFeedBackGainVector()
    {
        for (int channel = 0; channel < fdnSize; ++channel)
        {
            feedbackGainVector.set (channel, channelGainConversion (channel, overallGain));
        }
    }

    void updateFilterCoefficients()
    {
        if (isInitialized)
        {
            // update shelving filter parameters
            for (int channel = 0; channel < fdnSize; ++channel)
            {
                lowShelfFilters[channel]->setCoefficients (juce::IIRCoefficients::makeLowShelf (
                    spec.sampleRate,
                    juce::jmin (0.5 * spec.sampleRate,
                                static_cast<double> (lowShelfParameters.frequency)),
                    lowShelfParameters.q,
                    channelGainConversion (channel, lowShelfParameters.linearGain)));

                highShelfFilters[channel]->setCoefficients (juce::IIRCoefficients::makeHighShelf (
                    spec.sampleRate,
                    juce::jmin (0.5 * spec.sampleRate,
                                static_cast<double> (highShelfParameters.frequency)),
                    highShelfParameters.q,
                    channelGainConversion (channel, highShelfParameters.linearGain)));
            }

            juce::dsp::IIR::Coefficients<float>::Ptr tmpCoeffs;

            switch (hpFilterParameters.mode)
            {
                case 1:
                    tmpCoeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (
                        spec.sampleRate,
                        juce::jmin (0.5 * spec.sampleRate,
                                    static_cast<double> (hpFilterParameters.frequency)));

                    *hpCoefficients = *tmpCoeffs;
                    break;
                case 2:
                    tmpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                        spec.sampleRate,
                        juce::jmin (0.5 * spec.sampleRate,
                                    static_cast<double> (hpFilterParameters.frequency)),
                        hpFilterParameters.q);

                    *hpCoefficients = *tmpCoeffs;
                    break;

                case 3:
                    tmpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                        spec.sampleRate,
                        juce::jmin (0.5 * spec.sampleRate,
                                    static_cast<double> (hpFilterParameters.frequency)));
                    *hpCoefficients = *tmpCoeffs;
                    break;

                default:
                    tmpCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (
                        spec.sampleRate,
                        juce::jmin (0.5 * spec.sampleRate,
                                    static_cast<double> (hpFilterParameters.frequency)));

                    *hpCoefficients = *tmpCoeffs;
            }

            updateGuiCoefficients();
        }
    }

    void updateFdnSize (FdnSize newSize)
    {
        if (fdnSize != newSize)
        {
            const int diff = newSize - delayBufferVector.size();
            if (fdnSize < newSize)
            {
                for (int i = 0; i < diff; i++)
                {
                    delayBufferVector.add (new juce::AudioBuffer<float>());
                    highShelfFilters.add (new juce::IIRFilter());
                    lowShelfFilters.add (new juce::IIRFilter());
                    hpFilters.add (new juce::dsp::IIR::Filter<float> (hpCoefficients));
                    additionalHpFilters.add (new juce::dsp::IIR::Filter<float> (hpCoefficients));
                }
            }
            else
            {
                //TODO: what happens if newSize == 0?;
                delayBufferVector.removeLast (diff);
                highShelfFilters.removeLast (diff);
                lowShelfFilters.removeLast (diff);
                hpFilters.removeLast (diff);
                additionalHpFilters.removeLast (diff);
            }
        }
        delayPositionVector.resize (newSize);
        feedbackGainVector.resize (newSize);
        transferVector.resize (newSize);
        fdnSize = newSize;
    }
};
