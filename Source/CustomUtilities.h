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

//==============================================================================
/**
    A collection of utility functions.
*/
class CustomUtilities
{
public:
    /** Construct a normalisable range that is exponentially distributed.
     *  This is extremely useful for logarithmic sliders. */
    static inline NormalisableRange<float> getNormalisableRangeExp (float min, float max)
    {
        /* Credit for this function goes to JUCE forum user "JussiNeuralDSP".
         * See: https://forum.juce.com/t/logarithmic-slider-for-frequencies-iir-hpf/37569/9
         */
        jassert (min > 0.0f);
        jassert (max > 0.0f);
        jassert (min < max);

        float logmin = std::log (min);
        float logmax = std::log (max);
        float logrange = (logmax - logmin);

        jassert (logrange > 0.0f);

        return NormalisableRange<float> (
            min,
            max,
            [logmin, logrange] (float start, float end, float normalized)
            {
                normalized = std::max (0.0f, std::min (1.0f, normalized));
                float value = exp ((normalized * logrange) + logmin);
                return std::max (start, std::min (end, value));
            },
            [logmin, logrange] (float start, float end, float value)
            {
                value = std::max (start, std::min (end, value));
                float logvalue = std::log (value);
                return (logvalue - logmin) / logrange;
            },
            [] (float start, float end, float value)
            { return std::max (start, std::min (end, value)); });
    }
};
