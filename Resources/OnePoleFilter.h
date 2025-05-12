/*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Author: Felix Holzmüller
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
#define _USE_MATH_DEFINES
#include <math.h>

template <typename T>
class OnePoleFilter
{
public:
    OnePoleFilter()
    {
        a0 = 1;
        b1 = 0;
        z1 = 0;
        targetValue = 0;
    };
    OnePoleFilter (T newa0, T newb1)
    {
        a0 = newa0;
        b1 = newb1;
        z1 = 0;
        targetValue = 0;
    };

    void setCoefficients (T newa0, T newb1)
    {
        a0 = newa0;
        b1 = newb1;

        reset = true;
    };

    void setFrequency (T frequency, T sampleRate)
    {
        b1 = std::exp (-2.0 * M_PI * frequency / sampleRate);
        a0 = 1.0 - b1;

        reset = true;
    };

    void setTargetValue (T newTargetValue) { targetValue = newTargetValue; };

    T process (bool forceReset = false)
    {
        if (reset || forceReset)
        {
            z1 = targetValue;
            reset = false;
        }
        else
        {
            z1 = a0 * targetValue + b1 * z1;
        }
        return z1;
    }

    T process (T newTargetValue, bool forceReset = false)
    {
        targetValue = newTargetValue;
        return process (forceReset);
    }

private:
    T a0, b1;
    T z1;
    T targetValue;

    bool reset = true;
};