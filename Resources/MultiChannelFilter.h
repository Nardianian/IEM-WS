/*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Authors: Felix Holzmüller
 Copyright (c) 2024 - Institute of Electronic Music and Acoustics (IEM)
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
// Based on the MultiEQ by Daniel Rudrich

#pragma once

#include <JuceHeader.h>

enum FilterType
{
    FirstOrderHighPass,
    SecondOrderHighPass,
    LinkwitzRileyHighPass,
    LowShelf,
    PeakFilter,
    HighShelf,
    FirstOrderLowPass,
    SecondOrderLowPass,
    LinkwitzRileyLowPass,
    AllPass
};

struct FilterParameters
{
    FilterType type = AllPass;
    float frequency = 1000.0f;
    float linearGain = 1.0f;
    float q = 0.707f;
    bool enabled = false;
};

template <int numFilterBands, int maxChannels>
class MultiChannelFilter
{
#if JUCE_USE_SIMD
    using IIRfloat = juce::dsp::SIMDRegister<float>;
    static constexpr int IIRfloat_elements = juce::dsp::SIMDRegister<float>::size();
    static constexpr int nMaxSIMDFilters =
        ((maxChannels % IIRfloat_elements) == 0)
            ? (static_cast<int> (maxChannels / IIRfloat_elements))
            : (static_cast<int> (maxChannels / IIRfloat_elements) + 1);
#else /* !JUCE_USE_SIMD */
    using IIRfloat = float;
    static constexpr int IIRfloat_elements = 1;
    static constexpr int nMaxSIMDFilters = maxChannels;
#endif /* JUCE_USE_SIMD */

public:
    MultiChannelFilter()
    {
        // Create dummy filter coeffs
        for (int i = 0; i < 2; ++i)
        {
            additionalTempCoefficients[i] =
                juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0f, 20.0f);
            additionalProcessorCoefficients[i] =
                juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0f, 20.0f);
        }

        for (int i = 0; i < numFilterBands; ++i)
        {
            processorCoefficients[i] =
                juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0, 20.0f);
            tempCoefficients[i] = juce::dsp::IIR::Coefficients<float>::makeAllPass (48000.0, 20.0f);
        }

        for (int i = 0; i < numFilterBands; ++i)
        {
            filterArrays[i].clear();
            for (int ch = 0; ch < nMaxSIMDFilters; ++ch)
                filterArrays[i].add (
                    new juce::dsp::IIR::Filter<IIRfloat> (processorCoefficients[i]));
        }

        for (int i = 0; i < 2; ++i)
        {
            additionalFilterArrays[i].clear();
            for (int ch = 0; ch < nMaxSIMDFilters; ++ch)
                additionalFilterArrays[i].add (
                    new juce::dsp::IIR::Filter<IIRfloat> (additionalProcessorCoefficients[i]));
        }
    }
    ~MultiChannelFilter() {}

    void prepare (const juce::dsp::ProcessSpec spec, FilterParameters params[])
    {
        for (int f = 0; f < numFilterBands; ++f)
            updateFilterParams (params[f], f);

        prepare (spec);
    }

    void prepare (const juce::dsp::ProcessSpec spec)
    {
        sampleRate = spec.sampleRate;
        maxBlockSize = spec.maximumBlockSize;

        for (int f = 0; f < numFilterBands; ++f)
        {
            createFilterCoefficients (f);
        }
        copyFilterCoefficientsToProcessor();

        interleavedData.clear();

        for (int i = 0; i < nMaxSIMDFilters; ++i)
        {
            // reset filters
            for (int f = 0; f < numFilterBands; ++f)
            {
                filterArrays[f][i]->reset (IIRfloat (0.0f));
            }

            interleavedData.add (
                new juce::dsp::AudioBlock<IIRfloat> (interleavedBlockData[i], 1, maxBlockSize));
            clear (*interleavedData.getLast());
        }

        zero = juce::dsp::AudioBlock<float> (zeroData, IIRfloat_elements, maxBlockSize);
        zero.clear();
    }

    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        check();

        const auto& inputBlock = context.getInputBlock();
        auto&& outputBlock = context.getOutputBlock();

        const int L = inputBlock.getNumSamples();

        int nInChannels = inputBlock.getNumChannels();
        int nOutChannels = outputBlock.getNumChannels();

        const int maxNChIn = juce::jmin (nInChannels, nOutChannels, maxChannels);

        if (maxNChIn < 1)
            return;

        const int nSIMDFilters = 1 + (maxNChIn - 1) / IIRfloat_elements;

        // update iir filter coefficients
        if (userHasChangedFilterSettings.get())
            copyFilterCoefficientsToProcessor();

        using Format =
            juce::AudioData::Format<juce::AudioData::Float32, juce::AudioData::NativeEndian>;

        //interleave input data
        for (int i = 0; i < nSIMDFilters; ++i)
        {
            int nSubChannels = juce::jmin (IIRfloat_elements, maxNChIn - i * IIRfloat_elements);

            const auto& subInputBlock =
                inputBlock.getSubsetChannelBlock (i * IIRfloat_elements, nSubChannels);

            auto inChannels = prepareChannelPointers (subInputBlock);

            juce::AudioData::interleaveSamples (
                juce::AudioData::NonInterleavedSource<Format> { inChannels.data(),
                                                                IIRfloat_elements },
                juce::AudioData::InterleavedDest<Format> {
                    reinterpret_cast<float*> (interleavedData[i]->getChannelPointer (0)),
                    IIRfloat_elements },
                L);
        }

        // apply filters
        for (int f = 0; f < numFilterBands; ++f)
        {
            if (filterParameters[f].enabled)
            {
                for (int i = 0; i < nSIMDFilters; ++i)
                {
                    const IIRfloat* chPtr[1] = { interleavedData[i]->getChannelPointer (0) };
                    juce::dsp::AudioBlock<IIRfloat> ab (const_cast<IIRfloat**> (chPtr), 1, L);
                    juce::dsp::ProcessContextReplacing<IIRfloat> context (ab);
                    filterArrays[f][i]->process (context);
                }
            }
        }

        // check and apply additional filters (Linkwitz Riley -> two BiQuads)
        if (filterParameters[0].type == FilterType::LinkwitzRileyHighPass
            && filterParameters[0].enabled)
        {
            for (int i = 0; i < nSIMDFilters; ++i)
            {
                const IIRfloat* chPtr[1] = { chPtr[0] = interleavedData[i]->getChannelPointer (0) };
                juce::dsp::AudioBlock<IIRfloat> ab (const_cast<IIRfloat**> (chPtr), 1, L);
                juce::dsp::ProcessContextReplacing<IIRfloat> context (ab);
                additionalFilterArrays[0][i]->process (context);
            }
        }
        if (filterParameters[numFilterBands - 1].type == FilterType::LinkwitzRileyLowPass
            && filterParameters[numFilterBands - 1].enabled)
        {
            for (int i = 0; i < nSIMDFilters; ++i)
            {
                const IIRfloat* chPtr[1] = { interleavedData[i]->getChannelPointer (0) };
                juce::dsp::AudioBlock<IIRfloat> ab (const_cast<IIRfloat**> (chPtr), 1, L);
                juce::dsp::ProcessContextReplacing<IIRfloat> context (ab);
                additionalFilterArrays[1][i]->process (context);
            }
        }

        // deinterleave
        for (int i = 0; i < nSIMDFilters; ++i)
        {
            int nSubChannels = juce::jmin (IIRfloat_elements, maxNChIn - i * IIRfloat_elements);

            auto subOutBlock =
                outputBlock.getSubsetChannelBlock (i * IIRfloat_elements, nSubChannels);
            auto outChannels = prepareChannelPointers (subOutBlock);

            juce::AudioData::deinterleaveSamples (
                juce::AudioData::InterleavedSource<Format> {
                    reinterpret_cast<float*> (interleavedData[i]->getChannelPointer (0)),
                    IIRfloat_elements },
                juce::AudioData::NonInterleavedDest<Format> { outChannels.data(),
                                                              IIRfloat_elements },
                L);
            zero.clear();
        }
    }

    void updateFilterParams (FilterParameters newParams,
                             int filterIndex,
                             bool recalculateCoeffs = true)
    {
        filterParameters[filterIndex] = newParams;

        if (recalculateCoeffs)
        {
            createFilterCoefficients (filterIndex);
            userHasChangedFilterSettings = true;
        }
    }

    juce::dsp::IIR::Coefficients<double>::Ptr getCoefficientsForGui (const int filterIndex) const
    {
        return guiCoefficients[filterIndex];
    };

    void updateGuiCoefficients()
    {
        for (int f = 0; f < numFilterBands; ++f)
        {
            const auto frequency =
                juce::jmin (static_cast<float> (0.5 * sampleRate), filterParameters[f].frequency);

            switch (filterParameters[f].type)
            {
                case FilterType::FirstOrderHighPass:
                    guiCoefficients[f] =
                        juce::dsp::IIR::Coefficients<double>::makeFirstOrderHighPass (sampleRate,
                                                                                      frequency);
                    break;
                case FilterType::SecondOrderHighPass:
                    guiCoefficients[f] =
                        juce::dsp::IIR::Coefficients<double>::makeHighPass (sampleRate,
                                                                            frequency,
                                                                            filterParameters[f].q);
                    break;
                case FilterType::LinkwitzRileyHighPass:
                {
                    auto coeffs =
                        juce::dsp::IIR::Coefficients<double>::makeHighPass (sampleRate, frequency);
                    coeffs->coefficients =
                        FilterVisualizerHelper<double>::cascadeSecondOrderCoefficients (
                            coeffs->coefficients,
                            coeffs->coefficients);
                    guiCoefficients[f] = coeffs;
                    break;
                }
                case FilterType::LowShelf:
                    guiCoefficients[f] = juce::dsp::IIR::Coefficients<double>::makeLowShelf (
                        sampleRate,
                        frequency,
                        filterParameters[f].q,
                        filterParameters[f].linearGain);
                    break;
                case FilterType::PeakFilter:
                    guiCoefficients[f] = juce::dsp::IIR::Coefficients<double>::makePeakFilter (
                        sampleRate,
                        frequency,
                        filterParameters[f].q,
                        filterParameters[f].linearGain);
                    break;
                case FilterType::HighShelf:
                    guiCoefficients[f] = juce::dsp::IIR::Coefficients<double>::makeHighShelf (
                        sampleRate,
                        frequency,
                        filterParameters[f].q,
                        filterParameters[f].linearGain);
                    break;
                case FilterType::FirstOrderLowPass:
                    guiCoefficients[f] =
                        juce::dsp::IIR::Coefficients<double>::makeFirstOrderLowPass (sampleRate,
                                                                                     frequency);
                    break;
                case FilterType::SecondOrderLowPass:
                    guiCoefficients[f] =
                        juce::dsp::IIR::Coefficients<double>::makeLowPass (sampleRate,
                                                                           frequency,
                                                                           filterParameters[f].q);
                    break;
                case FilterType::LinkwitzRileyLowPass:
                {
                    auto coeffs =
                        juce::dsp::IIR::Coefficients<double>::makeLowPass (sampleRate, frequency);
                    coeffs->coefficients =
                        FilterVisualizerHelper<double>::cascadeSecondOrderCoefficients (
                            coeffs->coefficients,
                            coeffs->coefficients);
                    guiCoefficients[f] = coeffs;
                    break;
                }
                case FilterType::AllPass:
                    guiCoefficients[f] =
                        juce::dsp::IIR::Coefficients<double>::makeAllPass (sampleRate, frequency);
                    break;
                default:
                    break;
            }
        }
    }

private:
    void check()
    {
        for (int band = 0; band < numFilterBands; ++band)
            jassert (processorCoefficients[band] != nullptr);

        jassert (additionalProcessorCoefficients[0] != nullptr);
        jassert (additionalProcessorCoefficients[1] != nullptr);
    }

    static juce::Array<float> cascadeSecondOrderCoefficients (juce::Array<float>& c0,
                                                              juce::Array<float>& c1)
    {
        juce::Array<float> c12;
        c12.resize (9);
        const int o = 2;

        c12.setUnchecked (0, c0[0] * c1[0]);
        c12.setUnchecked (1, c0[0] * c1[1] + c0[1] * c1[0]);
        c12.setUnchecked (2, c0[0] * c1[2] + c0[1] * c1[1] + c0[2] * c1[0]);
        c12.setUnchecked (3, c0[1] * c1[2] + c0[2] * c1[1]);
        c12.setUnchecked (4, c0[2] * c1[2]);

        c12.setUnchecked (5, c1[1 + o] + c0[1 + o]);
        c12.setUnchecked (6, c1[2 + o] + c0[1 + o] * c1[1 + o] + c0[2 + o]);
        c12.setUnchecked (7, c0[1 + o] * c1[2 + o] + c0[2 + o] * c1[1 + o]);
        c12.setUnchecked (8, c0[2 + o] * c1[2 + o]);

        return c12;
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr createFilterCoefficients (const FilterType type,
                                                                              const float frequency,
                                                                              const float Q,
                                                                              const float gain)
    {
        const auto f = juce::jmin (static_cast<float> (0.5 * sampleRate), frequency);
        switch (type)
        {
            case FilterType::FirstOrderHighPass:
                return juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, f);
                break;
            case FilterType::SecondOrderHighPass:
                return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, Q);
                break;
            case FilterType::LowShelf:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, Q, gain);
                break;
            case FilterType::PeakFilter:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, f, Q, gain);
                break;
            case FilterType::HighShelf:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, Q, gain);
                break;
            case FilterType::FirstOrderLowPass:
                return juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sampleRate, f);
                break;
            case FilterType::SecondOrderLowPass:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, f, Q);
                break;
            default:
                return juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, f, Q);
                break;
        }
    }

    void createLinkwitzRileyFilter (const bool isUpperBand)
    {
        if (isUpperBand)
        {
            const auto frequency = juce::jmin (static_cast<float> (0.5 * sampleRate),
                                               filterParameters[numFilterBands - 1].frequency);
            tempCoefficients[numFilterBands - 1] =
                juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, frequency, 0.7071f);
            additionalTempCoefficients[1] = processorCoefficients[numFilterBands - 1];
        }
        else
        {
            const auto frequency =
                juce::jmin (static_cast<float> (0.5 * sampleRate), filterParameters[0].frequency);
            tempCoefficients[0] =
                juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, frequency, 0.7071f);
            additionalTempCoefficients[0] = processorCoefficients[0];
        }
    }

    void createFilterCoefficients (const int filterIndex)
    {
        if (filterIndex == 0 && filterParameters[0].type == FilterType::LinkwitzRileyHighPass)
        {
            createLinkwitzRileyFilter (false);
        }
        else if (filterIndex == numFilterBands - 1
                 && filterParameters[numFilterBands - 1].type == FilterType::LinkwitzRileyLowPass)
        {
            createLinkwitzRileyFilter (true);
        }
        else
        {
            tempCoefficients[filterIndex] =
                createFilterCoefficients (filterParameters[filterIndex].type,
                                          filterParameters[filterIndex].frequency,
                                          filterParameters[filterIndex].q,
                                          filterParameters[filterIndex].linearGain);
        }
    }

    void copyFilterCoefficientsToProcessor()
    {
        for (int b = 0; b < numFilterBands; ++b)
            *processorCoefficients[b] = *tempCoefficients[b];

        *additionalProcessorCoefficients[0] = *additionalTempCoefficients[0];
        *additionalProcessorCoefficients[1] = *additionalTempCoefficients[1];

        userHasChangedFilterSettings = false;
    }

    inline void clear (juce::dsp::AudioBlock<IIRfloat>& ab)
    {
        const int N = static_cast<int> (ab.getNumSamples()) * IIRfloat_elements;
        const int nCh = static_cast<int> (ab.getNumChannels());

        for (int ch = 0; ch < nCh; ++ch)
            juce::FloatVectorOperations::clear (
                reinterpret_cast<float*> (ab.getChannelPointer (ch)),
                N);
    }

    template <typename SampleType>
    auto prepareChannelPointers (const juce::dsp::AudioBlock<SampleType>& block)
    {
        std::array<SampleType*, IIRfloat_elements> result {};

        for (size_t ch = 0; ch < result.size(); ++ch)
            result[ch] = (ch < block.getNumChannels() ? block.getChannelPointer (ch)
                                                      : zero.getChannelPointer (ch));

        return result;
    }

    double sampleRate { 48000.0f };
    uint32_t maxBlockSize { 64 };

    // filter dummy for GUI
    juce::dsp::IIR::Coefficients<double>::Ptr guiCoefficients[numFilterBands];

    juce::dsp::IIR::Coefficients<float>::Ptr processorCoefficients[numFilterBands];
    juce::dsp::IIR::Coefficients<float>::Ptr additionalProcessorCoefficients[2];

    juce::dsp::IIR::Coefficients<float>::Ptr tempCoefficients[numFilterBands];
    juce::dsp::IIR::Coefficients<float>::Ptr additionalTempCoefficients[2];

    // data for interleaving audio
    juce::HeapBlock<char> interleavedBlockData[nMaxSIMDFilters], zeroData;
    juce::OwnedArray<juce::dsp::AudioBlock<IIRfloat>> interleavedData;
    juce::dsp::AudioBlock<float> zero;

    FilterParameters filterParameters[numFilterBands];

    // filters for processing
    juce::OwnedArray<juce::dsp::IIR::Filter<IIRfloat>> filterArrays[numFilterBands];
    juce::OwnedArray<juce::dsp::IIR::Filter<IIRfloat>> additionalFilterArrays[2];

    juce::Atomic<bool> userHasChangedFilterSettings { true };
};