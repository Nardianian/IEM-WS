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

WfsAudioProcessorEditor::WfsAudioProcessorEditor (WfsAudioProcessor& p, AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      valueTreeState (vts),
      footer (p.getOSCParameterInterface()),
      sourcePositionPad(audioProcessor.getWfsSpeakerArray())
{
    const int minWidth = minHeightInPixels * widthHeightRatio;

    setResizeLimits (minWidth, minHeightInPixels, minWidth * maxSizeRatio, minHeightInPixels * maxSizeRatio); // use this to create a resizable GUI
    setLookAndFeel (&globalLaF);

    /* Make sure the circle array stays circular. */
    getConstrainer()->setFixedAspectRatio(widthHeightRatio);

    /* Add title and footer. */
    addAndMakeVisible (&title);
    title.setTitle (String ("WaveField"), String ("Synthesizer"));
    title.setFont (globalLaF.robotoBold, globalLaF.robotoLight);
    addAndMakeVisible (&footer);

    const int textBoxWidth = 70;
    const int textBoxHeight = 15;

    /* Connect the title component's comboBoxes and the AudioProcessorValueTreeState. */
    cbInputChannelsSettingAttachment.reset (new ComboBoxAttachment (valueTreeState, "inputChannelsSetting", *title.getInputWidgetPtr()->getChannelsCbPointer()));
    cbOutputChannelsSettingAttachment.reset (new ComboBoxAttachment (valueTreeState, "outputChannelsSetting", *title.getOutputWidgetPtr()->getChannelsCbPointer()));

    /* Add all GUI parameters and set their settings. */
    addAndMakeVisible (cbLoudspeakerArrangement);
    cbLoudspeakerArrangement.setJustificationType (Justification::centred);
    cbLoudspeakerArrangement.addItem("Line", (int) LoudspeakerArrangementType::line);
    cbLoudspeakerArrangement.addItem("Circle", (int) LoudspeakerArrangementType::circle);
    cbLoudspeakerArrangementAttachment.reset (new ComboBoxAttachment (valueTreeState, "loudspeakerArrangement", cbLoudspeakerArrangement));

    addAndMakeVisible (slNLoudspeakers);
    slNLoudspeakers.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slNLoudspeakers.setColour(Slider::ColourIds::rotarySliderOutlineColourId, Colours::red);
    slNLoudspeakers.setTextBoxStyle(Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slNLoudspeakers.setNumDecimalPlacesToDisplay(0);
    slNLoudspeakersAttachment.reset (new SliderAttachment (valueTreeState, "nLoudspeakers", slNLoudspeakers));

    addAndMakeVisible (slLoudspeakerArrayLength);
    slLoudspeakerArrayLength.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slLoudspeakerArrayLength.setColour(Slider::ColourIds::rotarySliderOutlineColourId, Colours::green);
    slLoudspeakerArrayLength.setTextBoxStyle(Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slLoudspeakerArrayLength.setNumDecimalPlacesToDisplay(2);
    slLoudspeakerArrayLength.setTextValueSuffix("m");
    slLoudspeakerArrayLengthAttachment.reset (new SliderAttachment (valueTreeState, "loudspeakerArrayLength", slLoudspeakerArrayLength));

    addAndMakeVisible (slLoudspeakerRadius);
    slLoudspeakerRadius.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slLoudspeakerRadius.setColour(Slider::ColourIds::rotarySliderOutlineColourId, Colours::aqua);
    slLoudspeakerRadius.setTextBoxStyle(Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slLoudspeakerRadius.setNumDecimalPlacesToDisplay(2);
    slLoudspeakerRadius.setTextValueSuffix("m");
    slLoudspeakerArrayRadiusAttachment.reset (new SliderAttachment (valueTreeState, "loudspeakerRadius", slLoudspeakerRadius));

    addAndMakeVisible (slRotation);
    slRotation.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slRotation.setColour(Slider::ColourIds::rotarySliderOutlineColourId, Colours::aqua);
    slRotation.setTextBoxStyle(Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slRotation.setNumDecimalPlacesToDisplay(1);
    slRotation.setTextValueSuffix("deg");
    slRotationAttachment.reset (new SliderAttachment (valueTreeState, "rotation", slRotation));

    addAndMakeVisible (slZoom);
    slZoom.setSliderStyle(Slider::SliderStyle::LinearVertical);
    slZoom.setTextBoxStyle(Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slZoom.setNumDecimalPlacesToDisplay(2);
    slZoom.setTextValueSuffix("x");
    slZoomAttachment.reset (new SliderAttachment (valueTreeState, "zoom", slZoom));

    sourcePositionPad.setSourceRange(audioProcessor.getSourcePositionXRange(),
                                     audioProcessor.getSourcePositionYRange());
    sourcePositionPad.addElement(slListenerPositionX, slListenerPositionY, sourcePositionPad.getColours().source);
    sourcePositionPad.addElement(slSourcePositionX, slSourcePositionY, sourcePositionPad.getColours().listener);
    addAndMakeVisible(sourcePositionPad);
    slListenerPositionXAttachment.reset (new SliderAttachment (valueTreeState, "listenerPositionX", slListenerPositionX));
    slListenerPositionYAttachment.reset (new SliderAttachment (valueTreeState, "listenerPositionY", slListenerPositionY));
    slSourcePositionXAttachment.reset (new SliderAttachment (valueTreeState, "sourcePositionX", slSourcePositionX));
    slSourcePositionYAttachment.reset (new SliderAttachment (valueTreeState, "sourcePositionY", slSourcePositionY));

    addAndMakeVisible(lbLoudspeakerArrangement);
    lbLoudspeakerArrangement.setJustificationType(Justification::centred);
    lbLoudspeakerArrangement.setText("Arrangement", NotificationType::dontSendNotification);
    lbLoudspeakerArrangement.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    addAndMakeVisible(lbNLoudspeakers);
    lbNLoudspeakers.setJustificationType(Justification::centred);
    lbNLoudspeakers.setText("# Loudspeakers", NotificationType::dontSendNotification);
    lbNLoudspeakers.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    addAndMakeVisible(lbLoudspeakerArrayLength);
    lbLoudspeakerArrayLength.setJustificationType(Justification::centred);
    lbLoudspeakerArrayLength.setText("Array length", NotificationType::dontSendNotification);
    lbLoudspeakerArrayLength.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    addAndMakeVisible(lbLoudspeakerRadius);
    lbLoudspeakerRadius.setJustificationType(Justification::centred);
    lbLoudspeakerRadius.setText("Array radius", NotificationType::dontSendNotification);
    lbLoudspeakerRadius.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    addAndMakeVisible(lbRotation);
    lbRotation.setJustificationType(Justification::centred);
    lbRotation.setText("Rotation", NotificationType::dontSendNotification);
    lbRotation.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    addAndMakeVisible(lbZoom);
    lbZoom.setJustificationType(Justification::centred);
    lbZoom.setText("Zoom", NotificationType::dontSendNotification);
    lbZoom.setColour(Label::ColourIds::backgroundColourId, Colours::transparentWhite);

    /* Initialize previous variables for GUI updates. */
    previousLoudspeakerArrangementType = audioProcessor.getLoudspeakerArrangementType();
    previousZoom = *valueTreeState.getRawParameterValue("zoom");

    /* Start timer after everything is set up properly. */
    startTimer (20);
}

WfsAudioProcessorEditor::~WfsAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void WfsAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (globalLaF.ClBackground);
}

void WfsAudioProcessorEditor::resized()
{
    /* Set initial GUI sizes. */
    const int leftRightMargin = 30;
    const int headerHeight = 60;
    const int footerHeight = 25;

    const int comboBoxHeight = 15; // derived from TitleBar.h
    const int labelHeight = 20;

    /* Resize footer. */
    Rectangle<int> area (getLocalBounds());
    Rectangle<int> footerArea (area.removeFromBottom (footerHeight));
    footer.setBounds (footerArea);

    /* Resize header. */
    area.removeFromLeft (leftRightMargin);
    area.removeFromRight (leftRightMargin);
    Rectangle<int> headerArea = area.removeFromTop (headerHeight);
    title.setBounds (headerArea);

    /* Generate more areas for the GUI components. */
    area.removeFromTop (10);
    area.removeFromBottom (5);

    const int widthOfSlidersIn1stRow = area.getWidth() / 4;

    Rectangle<int> sliderRow = area.removeFromBottom (90);
    Rectangle<int> labelRow = sliderRow.removeFromBottom (labelHeight);
    labelRow.removeFromTop(3);

    Rectangle<int> comboBoxBounds = sliderRow.removeFromLeft(widthOfSlidersIn1stRow);
    comboBoxBounds.removeFromTop((comboBoxBounds.getHeight() - comboBoxHeight) / 2);
    comboBoxBounds.removeFromTop(20);

    /* Resize comboBox. */
    cbLoudspeakerArrangement.setBounds(comboBoxBounds);
    lbLoudspeakerArrangement.setBounds(labelRow.removeFromLeft(widthOfSlidersIn1stRow));

    /* Resize slider for number of loudspeakers. */
    slNLoudspeakers.setBounds(sliderRow.removeFromLeft(widthOfSlidersIn1stRow));
    lbNLoudspeakers.setBounds(labelRow.removeFromLeft(widthOfSlidersIn1stRow));

    /* Resize the sliders that depend on the loudspeaker arrangement type.
     * Only make the ones visible which are relevant for the current arrangement type. */
    if (audioProcessor.getLoudspeakerArrangementType() == LoudspeakerArrangementType::line)
    {
        slLoudspeakerArrayLength.setBounds(sliderRow.removeFromLeft(widthOfSlidersIn1stRow));
        lbLoudspeakerArrayLength.setBounds(labelRow.removeFromLeft(widthOfSlidersIn1stRow));

        slLoudspeakerArrayLength.setVisible(true);
        lbLoudspeakerArrayLength.setVisible(true);
        slLoudspeakerRadius.setVisible(false);
        slRotation.setVisible(false);
        lbLoudspeakerRadius.setVisible(false);
        lbRotation.setVisible(false);
    }
    else if (audioProcessor.getLoudspeakerArrangementType() == LoudspeakerArrangementType::circle)
    {
        slLoudspeakerRadius.setBounds(sliderRow.removeFromLeft(widthOfSlidersIn1stRow));
        slRotation.setBounds(sliderRow.removeFromLeft(widthOfSlidersIn1stRow));
        lbLoudspeakerRadius.setBounds(labelRow.removeFromLeft(widthOfSlidersIn1stRow));
        lbRotation.setBounds(labelRow.removeFromLeft(widthOfSlidersIn1stRow));

        slLoudspeakerRadius.setVisible(true);
        slRotation.setVisible(true);
        lbLoudspeakerRadius.setVisible(true);
        lbRotation.setVisible(true);
        slLoudspeakerArrayLength.setVisible(false);
        lbLoudspeakerArrayLength.setVisible(false);
        slLoudspeakerArrayLength.removeFromDesktop();
    }
    else
    {
        jassertfalse;
    }

    area.removeFromBottom(10);

    /* Resize zoom slider. */
    Rectangle<int> zoomSliderArea = area.removeFromRight(50);
    Rectangle<int> zoomSliderLabelArea = zoomSliderArea.removeFromBottom (labelHeight);
    zoomSliderLabelArea.removeFromTop (3);
    slZoom.setBounds (zoomSliderArea);
    lbZoom.setBounds (zoomSliderLabelArea);

    area.removeFromRight(15);
    area.reduced(0, 5);

    /* Resize source and listener position pad. */
    slSourcePositionX.setBounds(area);
    slSourcePositionY.setBounds(area);
    sourcePositionPad.setZoom(*valueTreeState.getRawParameterValue("zoom"));
    sourcePositionPad.setBounds(area);
}

void WfsAudioProcessorEditor::timerCallback()
{
    /* Update available input/output channel counts in titleBar widget. */
    title.setMaxSize (audioProcessor.getMaxSize());

    /* Update whole GUI if the loudspeaker arrangement type has changed. */
    if (previousLoudspeakerArrangementType != audioProcessor.getLoudspeakerArrangementType())
    {
        resized();
        previousLoudspeakerArrangementType = audioProcessor.getLoudspeakerArrangementType();
    }

    /* Redraw the loudspeaker array if the PluginProcessor says so. Also update all variables that come
     * with a new loudspeaker array. */
    if (audioProcessor.redrawLoudspeakers)
    {
        audioProcessor.redrawLoudspeakers = false;

        float zoom = *valueTreeState.getRawParameterValue("zoom");

        sourcePositionPad.setZoom(zoom);
        sourcePositionPad.setNLoudspeakers(*valueTreeState.getRawParameterValue("nLoudspeakers"));
        sourcePositionPad.setLoudspeakerArrayLength(*valueTreeState.getRawParameterValue("loudspeakerArrayLength"));
        sourcePositionPad.setLoudspeakerRadius(*valueTreeState.getRawParameterValue("loudspeakerRadius"));
        sourcePositionPad.setRotation(*valueTreeState.getRawParameterValue("rotation"));
        sourcePositionPad.setLoudspeakerArrangementType(audioProcessor.getLoudspeakerArrangementType());

        float zoomDifference = zoom - previousZoom;
        previousZoom = zoom;

        slSourcePositionX.setValue(slSourcePositionX.getValue() * (1 + zoomDifference));
        slSourcePositionY.setValue(slSourcePositionY.getValue() * (1 + zoomDifference));
        slListenerPositionX.setValue(slListenerPositionX.getValue() * (1 + zoomDifference));
        slListenerPositionY.setValue(slListenerPositionY.getValue() * (1 + zoomDifference));

        sourcePositionPad.repaintBackground();
        resized();
    }

    if (audioProcessor.redrawXYPad)
    {
        audioProcessor.redrawXYPad = false;

        sourcePositionPad.repaint(sourcePositionPad.getBounds());
    }
}
