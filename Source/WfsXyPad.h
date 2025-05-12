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
#include "WfsXyPadBackground.h"

using namespace juce;

//==============================================================================
/**
    The XYPad used to drag and paint the source and listener positions.

    To connect to its background, this class holds the background as member. However,
    the background is only drawn if needed, to save ressources.
*/
class WfsXyPad : public Component
{
    struct xySlidersAndColour
    {
        Slider* xSlider = nullptr;
        Slider* ySlider = nullptr;
        Colour colour;
    };

public:
    WfsXyPad (Array<WfsSpeakerProcessor*>& wfsSpeakerArrayToUse) :
        xRangeWithZoom1 (-10.0f, 10.0f),
        yRangeWithZoom1 (-5.0f, 5.0f),
        background (wfsSpeakerArrayToUse)
    {
        background.setBounds (getLocalBounds());
        addAndMakeVisible (background);
        background.addMouseListener (this, false);
    }
    ~WfsXyPad() override = default;

    void paintOverChildren (Graphics& g) override
    {
        /* Define GUI sizes. */
        const int edgeDistanceInPixels = 10;
        const int positionCircleRadius = 12;

        int topLeftX = plotArea.getX();
        int bottomY = plotArea.getY() + plotArea.getHeight();

        int size = elements.size();

        for (int i = 0; i < size; ++i)
        {
            bool isActive = (activeElem == i);

            xySlidersAndColour handle = elements.getReference (i);

            /* Get range and positions. */
            Range<double> xRange = handle.xSlider->getRange();
            Range<double> yRange = handle.ySlider->getRange();
            float xPosInPixels =
                topLeftX
                + (handle.xSlider->getValue() - xRange.getStart()) * width / xRange.getLength();
            float yPosInPixels =
                bottomY
                - (handle.ySlider->getValue() - yRange.getStart()) * height / yRange.getLength();

            if (i == LISTENER_INDEX)
            {
                if (background.loudspeakerArrangementType == LoudspeakerArrangementType::line)
                {
                    /* Limit listener position. */
                    yPosInPixels = jmax (yPosInPixels, bottomY - height / 2);

                    /* Draw reference line. */
                    g.setColour (background.colours.listener.withBrightness (1.5));
                    Line<float> referenceLine (plotArea.getX() + edgeDistanceInPixels,
                                               yPosInPixels,
                                               plotArea.getX() + plotArea.getWidth()
                                                   - edgeDistanceInPixels,
                                               yPosInPixels);
                    g.drawDashedLine (referenceLine,
                                      Array<float> (6, 6).getRawDataPointer(),
                                      2,
                                      2.0f);
                }
                else
                {
                    /* TODO limit position for circle arrangement. */
                }

                g.setColour (background.colours.listener);
            }
            else
            {
                g.setColour (background.colours.source);
            }

            /* Draw positional circle. */
            Path path;
            path.addEllipse (xPosInPixels - positionCircleRadius,
                             yPosInPixels - positionCircleRadius,
                             positionCircleRadius * 2,
                             positionCircleRadius * 2);
            g.fillPath (path);

            /* Annotate the circle with either "L" for Listener or "S" for Source. */
            g.setColour (background.colours.text);
            if (i == LISTENER_INDEX)
                g.drawText ("L", path.getBounds(), Justification::centred);
            else
                g.drawText ("S", path.getBounds(), Justification::centred);

            /* Highlight the circle if it is currently active. */
            if (isActive)
            {
                g.setColour (background.colours.activePath);
                g.strokePath (path, PathStrokeType (2.0f));
            }
        }
    }

    /* Resizes the position circles if called. */
    void resized() override
    {
        Rectangle<int> bounds = getLocalBounds();
        background.setBounds (bounds);

        /* Make sure the circles don't go over the source pad edges. */
        bounds.reduce (12, 12);

        plotArea = bounds;
        width = bounds.getWidth();
        height = bounds.getHeight();
    }

    void mouseMove (const MouseEvent& event) override
    {
        Point<int> pos = event.getPosition();
        int oldActiveElem = activeElem;
        activeElem = -1;

        int topLeftX = plotArea.getX();
        int bottomY = plotArea.getY() + plotArea.getHeight();

        for (int i = elements.size() - 1; i >= 0; --i)
        {
            xySlidersAndColour handle = elements.getReference (i);

            Range<double> xRange = handle.xSlider->getRange();
            Range<double> yRange = handle.ySlider->getRange();

            float xPos =
                topLeftX
                + (handle.xSlider->getValue() - xRange.getStart()) * width / xRange.getLength();
            float yPos =
                bottomY
                - (handle.ySlider->getValue() - yRange.getStart()) * height / yRange.getLength();

            /* Limit listener position depending on arrangement type. */
            if (i == LISTENER_INDEX)
            {
                if (background.loudspeakerArrangementType == LoudspeakerArrangementType::line)
                {
                    yPos = jmax (yPos, bottomY - height / 2);
                }
                else
                {
                    /* TODO limit position for circle arrangement. */
                }
            }

            if (pos.getDistanceSquaredFrom (Point<float> (xPos, yPos).toInt()) < 80)
            {
                activeElem = i;
                break;
            }
        }

        if (oldActiveElem != activeElem)
            repaint (getLocalBounds());
    }

    void mouseDrag (const MouseEvent& event) override
    {
        Point<int> pos = event.getPosition() - plotArea.getTopLeft();

        if (activeElem != -1 && elements.size() - 1 >= activeElem)
        {
            xySlidersAndColour handle = elements.getReference (activeElem);
            Range<double> xRange = handle.xSlider->getRange();
            Range<double> yRange = handle.ySlider->getRange();

            handle.xSlider->setValue (xRange.getLength() * pos.x / width + xRange.getStart());
            handle.ySlider->setValue (yRange.getLength() * (height - pos.y) / height
                                      + yRange.getStart());

            /* TODO There's a dead zone without this call to repaint(). Why? */
            repaint (getLocalBounds());
        }
    }

    void mouseUp (const MouseEvent& event) override
    {
        activeElem = -1;
        repaint (getLocalBounds());
    }

    void addElement (Slider& newXSlider, Slider& newYSlider, Colour newColour)
    {
        elements.add ({ &newXSlider, &newYSlider, newColour });
    }

    /* Setter functions for all loudspeaker parameters. */
    void setZoom (float zoomToUse)
    {
        zoom = zoomToUse;
        background.zoom = zoomToUse;
    }
    void setNLoudspeakers (int nLoudspeakersToUse)
    {
        background.nLoudspeakers = nLoudspeakersToUse;
    }
    void setLoudspeakerArrayLength (float loudspeakerArrayLengthToUse)
    {
        background.loudspeakerArrayLength = loudspeakerArrayLengthToUse;
    }
    void setLoudspeakerRadius (float loudspeakerRadiusToUse)
    {
        background.loudspeakerRadius = loudspeakerRadiusToUse;
    }
    void setRotation (float rotationToUse) { background.rotation = rotationToUse; }
    void setLoudspeakerArrangementType (LoudspeakerArrangementType loudspeakerArrangementTypeToUse)
    {
        background.loudspeakerArrangementType = loudspeakerArrangementTypeToUse;
    }
    void setSourceRange (Range<float> newXRange, Range<float> newYRange)
    {
        xRangeWithZoom1 = newXRange;
        yRangeWithZoom1 = newYRange;
        background.xRangeWithZoom1 = newXRange;
        background.yRangeWithZoom1 = newYRange;
    }

    /* Repaint the WfsXyPadBackground if needed. */
    void repaintBackground() { background.repaint (getLocalBounds()); }

    /* The colour definitions of the background are used in the WfsXyPad and the PluginEditor too. */
    WfsXyPadBackground::WfsXyPadColours& getColours() { return background.colours; }

protected:
    /* The slider elements. representing source and listener positions. */
    Array<xySlidersAndColour> elements;
    int activeElem = -1;

    /* GUI sizes. */
    Rectangle<int> plotArea;
    float width;
    float height;

private:
    /* The background of the WfsXyPad. */
    WfsXyPadBackground background;

    /* Slider ranges in meters. */
    Range<float> xRangeWithZoom1;
    Range<float> yRangeWithZoom1;

    /* The current zoom factor of the GUI: */
    float zoom;
};