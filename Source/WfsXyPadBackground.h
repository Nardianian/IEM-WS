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

#include "WfsSpeakerProcessor.h"

using namespace juce;

const int LISTENER_INDEX = 0;
const int SOURCE_INDEX = 1;

//==============================================================================
/**
    The background for the WfsXyPad.

    It draws the following elements:
      - rectangular boxes, whose sizes scale with changing zoom factor
      - a coordinate system
      - the loudspeaker array

    Drawing the background is resource-expensive. Due to that, the background is
    separated from the WfsXyPad (source and listener positions).
*/
class WfsXyPadBackground : public Component
{
    /** Define a loudspeaker position type. */
    typedef struct LoudspeakerPosition
    {
        int xInPixels;
        int yInPixels;
        float rotationInDegrees;

        LoudspeakerPosition (int xInPixelsToUse, int yInPixelsToUse, float rotationInDegreesToUse)
        {
            xInPixels = xInPixelsToUse;
            yInPixels = yInPixelsToUse;
            rotationInDegrees = rotationInDegreesToUse;
        }
    } LoudspeakerPosition;

public:
    /** Define the colours all together in one struct. */
    typedef struct WfsXyPadColours
    {
        Colour source = Colours::aqua;
        Colour listener = Colours::red;
        Colour text = Colours::black;
        Colour textBackground = Colours::whitesmoke;
        Colour activePath = Colours::white;
        Colour loudspeakerBorder = Colours::whitesmoke;
        Colour axes = Colours::snow;
        Colour background = Colours::steelblue;
    } WfsXyPadColours;

    WfsXyPadBackground (Array<WfsSpeakerProcessor*>& wfsSpeakerArrayToUse) :
        xRangeWithZoom1 (-10.0f, 10.0f),
        yRangeWithZoom1 (-7.5f, 7.5f),
        loudspeakerArrangementType (LoudspeakerArrangementType::line),
        nLoudspeakers (10),
        loudspeakerArrayLength (5.0f),
        loudspeakerRadius (5.0f),
        rotation (0.0f),
        wfsSpeakerArray (wfsSpeakerArrayToUse)
    {
        /* Don't redraw the background each time the source and listener positions are updated. */
        setBufferedToImage (true);
    }
    ~WfsXyPadBackground() override = default;

    /** Draw everything if the pad is created anew. */
    void paint (Graphics& g) override
    {
        drawBox (g);
        drawCoordinates (g);
        drawLoudspeakers (g);
    }

    /** Resize all components. */
    void resized() override
    {
        Rectangle<int> bounds = getLocalBounds();

        bounds.reduce (12, 12);
    }

    void drawBox (Graphics& g)
    {
        Path boxPath;
        Rectangle<int> bounds = getLocalBounds();
        Array<float> boxBoundsXInMeter, boxBoundsYInMeter;

        /* Define color multiplier factor limits. */
        float baseAlpha = 0.2;
        float maxAlpha = 0.8;

        float tmp1, tmp2;

        /* Define bounds of rectangular box in meter, depending on zoom.
         * Do so due to the zoom effect, that is triggered when the zoom slider is dragged. */
        if ((zoom >= 0.1f) && (zoom < 0.5f))
        {
            boxBoundsXInMeter.add (1.0f, 5.0f, 10.0f, 50.0f);
            boxBoundsYInMeter.add (0.5f, 2.5f, 5.0f, 25.5f);
        }
        else if ((zoom >= 0.5f) && (zoom < 1.0f))
        {
            boxBoundsXInMeter.add (0.5f, 1.0f, 5.0f, 10.0f);
            boxBoundsYInMeter.add (0.25f, 0.5f, 2.5f, 5.0f);
        }
        else if ((zoom >= 1.0f) && (zoom < 5.0f))
        {
            boxBoundsXInMeter.add (0.1f, 0.5f, 1.0f, 5.0f);
            boxBoundsYInMeter.add (0.05f, 0.25f, 0.5f, 2.5f);
        }
        else if ((zoom >= 5.0f) && (zoom < 10.0f))
        {
            boxBoundsXInMeter.add (0.05f, 0.1f, 0.5f, 1.0f);
            boxBoundsYInMeter.add (0.025f, 0.05f, 0.25f, 0.5f);
        }
        else
        {
            jassertfalse;
        }

        /* First fill whole bounds. */
        boxPath.addRectangle (bounds);
        g.setColour (colours.background.withMultipliedAlpha (baseAlpha));
        g.fillPath (boxPath);

        /* Draw each rectangle in the pad background.
         * The color and the size of each rectangle depends on the zoom factor. */
        float alphaMultiplier, rationalX, rationalY;
        for (int iStep = 0; iStep < boxBoundsXInMeter.size(); ++iStep)
        {
            boxPath.clear();
            bounds = getLocalBounds();

            /* Get the amount of space the rectangle uses in the GUI. */
            rationalX = boxBoundsXInMeter[iStep] / (xRangeWithZoom1.getLength() / zoom / 2.0f);
            rationalY = boxBoundsYInMeter[iStep] / (yRangeWithZoom1.getLength() / zoom / 2.0f);

            /* Define the bounds for the rectangle. */
            bounds.reduce ((static_cast<float> (bounds.getWidth()) / 2.0f) * (1.0f - rationalX),
                           (static_cast<float> (bounds.getHeight()) / 2.0f) * (1.0f - rationalY));
            boxPath.addRectangle (bounds);

            /* Calculate the color alpha multiplier. */
            tmp1 =
                pow (static_cast<float> (iStep + 1) / static_cast<float> (boxBoundsXInMeter.size()),
                     1.5);
            tmp2 = pow (static_cast<float> (boxBoundsXInMeter.size()), 1.5);
            alphaMultiplier = baseAlpha + (maxAlpha - baseAlpha) * (tmp1 / tmp2);

            /* Draw the rectangle. */
            g.setColour (colours.background.withMultipliedAlpha (alphaMultiplier));
            g.fillPath (boxPath);
        }

        /* Draw border around background. */
        g.setColour (Colours::white);
        g.drawRect (getLocalBounds(), 2.0f);
    }

    void drawCoordinates (Graphics& g)
    {
        const int edgeDistanceInPixels = 10;
        const int labelDistanceInPixels = 50;
        const int labelLength = 10;
        const int distanceOfLabelToText = 2;
        const int labelTextHeight = 10;

        Line<float> label;
        Rectangle<int> textBounds;

        g.setColour (colours.axes);

        /* Draw axes */
        Rectangle<int> axesBounds = getLocalBounds();
        axesBounds.reduce (edgeDistanceInPixels, edgeDistanceInPixels);

        Line<float> xAxes (axesBounds.getRight() - axesBounds.getWidth(),
                           axesBounds.getCentreY(),
                           axesBounds.getRight(),
                           axesBounds.getCentreY());
        Line<float> yAxes (axesBounds.getCentreX(),
                           axesBounds.getBottom() - axesBounds.getHeight(),
                           axesBounds.getCentreX(),
                           axesBounds.getBottom());
        g.drawLine (xAxes, 1.0f);
        g.drawLine (yAxes, 1.0f);

        /* Calculate the positions of the x-axis ticks. */
        std::vector<int> xCoordinatesOfLabels =
            calculateCenteredLinearRange (axesBounds.getHorizontalRange(),
                                          axesBounds.getCentreX(),
                                          labelDistanceInPixels);

        /* Draw x-axis ticks and labels (in meter). */
        for (auto& x : xCoordinatesOfLabels)
        {
            label = Line<float> (x,
                                 axesBounds.getCentreY() - labelLength / 2,
                                 x,
                                 axesBounds.getCentreY() + labelLength / 2);
            g.drawLine (label);

            textBounds = Rectangle<int> (label.getStartX() - labelDistanceInPixels / 2,
                                         label.getEndY() + distanceOfLabelToText,
                                         labelDistanceInPixels,
                                         labelTextHeight);
            Point<float> realCoordinates =
                convertPointInPixelsToMeter (Point<int> (x, axesBounds.getCentreY()), axesBounds);
            g.drawText (String (realCoordinates.getX(), 1) + "m",
                        textBounds,
                        Justification::centred);
        }

        /* Calculate the positions of the y-axis ticks. */
        std::vector<int> yCoordinatesOfLabels =
            calculateCenteredLinearRange (axesBounds.getVerticalRange(),
                                          axesBounds.getCentreY(),
                                          labelDistanceInPixels);

        /* Draw y-axis ticks and labels (in meter). */
        for (auto& y : yCoordinatesOfLabels)
        {
            label = Line<float> (axesBounds.getCentreX() - labelLength / 2,
                                 y,
                                 axesBounds.getCentreX() + labelLength / 2,
                                 y);
            g.drawLine (label);

            textBounds = Rectangle<int> (label.getEndX() + distanceOfLabelToText,
                                         y - labelTextHeight / 2,
                                         labelDistanceInPixels,
                                         labelTextHeight);
            Point<float> realCoordinates =
                convertPointInPixelsToMeter (Point<int> (axesBounds.getCentreX(), y), axesBounds);
            g.drawText (String (-realCoordinates.getY(), 1) + "m",
                        textBounds,
                        Justification::centred);
        }
    }

    void drawLoudspeakers (Graphics& g)
    {
        /* Set loudspeaker dimensions in GUI. */
        const int rectangleWidth = 12;
        const int rectangleHeight = 9;

        const int trapeziumWidth = 22;
        const int trapeziumHeight = rectangleHeight;

        const int lineThickness = 2;

        int x, y;
        float rotationInRad;
        Path lsRectanglePath, lsTrapeziumPath, lsNumberPath;
        GlyphArrangement ga;

        std::vector<LoudspeakerPosition> loudspeakerPositions =
            calculateLoudspeakerPositionsInPixels();
        LoudspeakerPosition loudspeaker = loudspeakerPositions[0];

        Rectangle<int> lsRectangle;

        for (int iLoudspeaker = 0; iLoudspeaker < loudspeakerPositions.size(); ++iLoudspeaker)
        {
            /* Init variables. */
            loudspeaker = loudspeakerPositions[iLoudspeaker];
            lsRectanglePath.clear();
            lsTrapeziumPath.clear();
            lsNumberPath.clear();
            ga.clear();

            x = loudspeaker.xInPixels;
            y = loudspeaker.yInPixels;

            /* Define the rotational transformation. */
            rotationInRad = -MathConstants<float>::pi * loudspeaker.rotationInDegrees / 180.0f;
            AffineTransform rotationTransformation =
                AffineTransform::rotation (rotationInRad, x, y);

            /* Create the loudspeaker rectangle path. */
            lsRectangle = Rectangle<int> (x - rectangleWidth / 2,
                                          y - 2 * rectangleHeight,
                                          rectangleWidth,
                                          rectangleHeight);
            lsRectanglePath.addRectangle (lsRectangle);
            lsRectanglePath.closeSubPath();

            /* Create the loudspeaker trapezium path. */
            lsTrapeziumPath.startNewSubPath (x + rectangleWidth / 2, y - rectangleHeight);
            lsTrapeziumPath.lineTo (x + trapeziumWidth / 2, y);
            lsTrapeziumPath.lineTo (x - trapeziumWidth / 2, y);
            lsTrapeziumPath.lineTo (x - rectangleWidth / 2, y - rectangleHeight);
            lsTrapeziumPath.closeSubPath();

            /* Assume that the first loudspeaker position is at 270°. */
            if (loudspeakerArrangementType == LoudspeakerArrangementType::circle)
            {
                /* Draw loudspeaker. */
                g.setColour (colours.loudspeakerBorder);
                g.strokePath (lsRectanglePath,
                              PathStrokeType (lineThickness),
                              rotationTransformation);
                g.strokePath (lsTrapeziumPath,
                              PathStrokeType (lineThickness),
                              rotationTransformation);

                g.setColour (colours.loudspeakerBorder.withMultipliedAlpha (0.5f));
                g.fillPath (lsRectanglePath, rotationTransformation);
                g.fillPath (lsTrapeziumPath, rotationTransformation);
            }
            else if (loudspeakerArrangementType == LoudspeakerArrangementType::line)
            {
                /* Draw loudspeaker. */
                g.setColour (colours.loudspeakerBorder);
                g.strokePath (lsRectanglePath, PathStrokeType (lineThickness));
                g.strokePath (lsTrapeziumPath, PathStrokeType (lineThickness));

                g.setColour (colours.loudspeakerBorder.withMultipliedAlpha (0.5f));
                g.fillPath (lsRectanglePath);
                g.fillPath (lsTrapeziumPath);
            }

            /* Draw loudspeaker number. */
            g.setColour (colours.text);
            ga.addFittedText (Font (12.0f),
                              String (iLoudspeaker + 1),
                              lsRectangle.getX(),
                              lsRectangle.getY(),
                              lsRectangle.getWidth(),
                              lsRectangle.getHeight(),
                              Justification::centred,
                              1);
            ga.createPath (lsNumberPath);

            /* Rotate the loudspeaker only for circle arrangement. */
            if (loudspeakerArrangementType == LoudspeakerArrangementType::circle)
                lsNumberPath.applyTransform (rotationTransformation);

            g.fillPath (lsNumberPath);
        }
    }

    /* This function can label the edge dimensions of the source pad. */
    void labelBoxEdges (Graphics& g)
    {
        String text;
        const int edgeDistanceInPixels = 5;

        double xArray[4] = { xRangeWithZoom1.getStart(),
                             xRangeWithZoom1.getEnd(),
                             xRangeWithZoom1.getEnd(),
                             xRangeWithZoom1.getStart() };
        double yArray[4] = { yRangeWithZoom1.getStart(),
                             yRangeWithZoom1.getEnd(),
                             yRangeWithZoom1.getEnd(),
                             yRangeWithZoom1.getStart() };
        int justifications[4] = { Justification::bottomLeft,
                                  Justification::bottomRight,
                                  Justification::topRight,
                                  Justification::topLeft };

        Rectangle<int> bounds = getLocalBounds();
        bounds.removeFromLeft (edgeDistanceInPixels);
        bounds.removeFromRight (edgeDistanceInPixels);
        bounds.removeFromBottom (edgeDistanceInPixels);
        bounds.removeFromTop (edgeDistanceInPixels);

        for (int i = 0; i < 4; i++)
        {
            text = "(" + Point<double> (xArray[i], yArray[i]).toString() + ")";
            g.drawText (text, bounds, justifications[i]);
        }
    }

    Point<float> convertPointInPixelsToMeter (Point<int> pointInPixels,
                                              Rectangle<int> guiBounds) const
    {
        float relativeX = static_cast<float> (pointInPixels.getX() - guiBounds.getTopLeft().getX())
                          / static_cast<float> (guiBounds.getWidth());
        float relativeY = static_cast<float> (pointInPixels.getY() - guiBounds.getTopLeft().getY())
                          / static_cast<float> (guiBounds.getHeight());

        Point<float> pointInMeter (
            xRangeWithZoom1.getStart() + xRangeWithZoom1.getLength() * relativeX,
            yRangeWithZoom1.getStart() + yRangeWithZoom1.getLength() * relativeY);

        /* Take zoom factor into account. */
        pointInMeter /= zoom;

        return pointInMeter;
    }

    Point<float> convertPointInMeterToPixels (Point<float> pointInMeter,
                                              Rectangle<int> guiBounds) const
    {
        /* Normalize pointInMeter to zoom of 1. */
        pointInMeter *= zoom;

        /* NOTE: y-axis is kind of flipped, so we need to invert the Y point. */
        float relativeX =
            (pointInMeter.getX() - xRangeWithZoom1.getStart()) / xRangeWithZoom1.getLength();
        float relativeY = (static_cast<float> (-pointInMeter.getY()) - yRangeWithZoom1.getStart())
                          / yRangeWithZoom1.getLength();

        Point<float> pointInPixels (
            guiBounds.getTopLeft().getX() + guiBounds.getWidth() * relativeX,
            guiBounds.getTopLeft().getY() + guiBounds.getHeight() * relativeY);

        return pointInPixels;
    }

    static std::vector<int> calculateCenteredLinearRange (Range<int> range,
                                                          int centerValue,
                                                          int distance,
                                                          bool addCenterValue = false)
    {
        std::vector<int> values;

        int minValue = centerValue;
        while (minValue > range.getStart())
            minValue -= distance;

        /* Found minValue, now create linear range. */
        int value = minValue;
        while (value < range.getEnd())
        {
            if (value != centerValue || addCenterValue)
                values.push_back (value);

            value += distance;
        }

        return values;
    }

    std::vector<LoudspeakerPosition> calculateLoudspeakerPositionsInPixels()
    {
        /* It's possible that there are many loudspeakers. If their positions are all
         * treated as integers, the rounding error will sum up until the last one.
         * To avoid this error, positions are floats during calculations and rounded and
         * converted to int when assigned to the vector.
         */
        std::vector<LoudspeakerPosition> loudspeakerPositions;

        /* Define GUI variables. */
        Rectangle<int> bounds = getLocalBounds();
        float positionalRotation = 0.0f;
        float baseRotationCircle = 0.0f;
        float baseRotationLine = 0.0f;

        Point<float> positionInMeters, positionInPixels;
        for (int iLoudspeaker = 0; iLoudspeaker < wfsSpeakerArray.size(); ++iLoudspeaker)
        {
            positionInMeters = Point<float> (wfsSpeakerArray[iLoudspeaker]->getXInMeters(),
                                             wfsSpeakerArray[iLoudspeaker]->getYInMeters());
            positionInPixels = convertPointInMeterToPixels (positionInMeters, bounds);

            if (loudspeakerArrangementType == LoudspeakerArrangementType::circle)
            {
                positionalRotation = rotation + baseRotationCircle
                                     + static_cast<float> (iLoudspeaker)
                                           / static_cast<float> (wfsSpeakerArray.size()) * 360.0f;
            }
            else
            {
                positionalRotation = baseRotationLine;
            }

            loudspeakerPositions.emplace_back (static_cast<int> (positionInPixels.getX()),
                                               static_cast<int> (positionInPixels.getY()),
                                               positionalRotation);
        }

        return loudspeakerPositions;
    }

    WfsXyPadColours colours;

    LoudspeakerArrangementType loudspeakerArrangementType;
    int nLoudspeakers;
    float loudspeakerArrayLength;
    float loudspeakerRadius;
    float rotation;
    float zoom;

    /* Coordinate ranges in meters, normalized to a neutral zoom factor of 1. */
    Range<float> xRangeWithZoom1;
    Range<float> yRangeWithZoom1;

private:
    /* For the loudspeaker positions the background needs to know the loudspeaker array. */
    Array<WfsSpeakerProcessor*>& wfsSpeakerArray;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WfsXyPadBackground)
};
