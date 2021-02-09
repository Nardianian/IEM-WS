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
#include "PluginProcessor.h"

/** Include Plugin Design Essentials. */
#include "../Resources/lookAndFeel/IEM_LaF.h"
#include "../Resources/customComponents/TitleBar.h"

/** Include Custom Components. */
#include "../Resources/customComponents/ReverseSlider.h"
#include "../Resources/customComponents/SimpleLabel.h"
#include "WfsXyPad.h"

typedef ReverseSlider::SliderAttachment SliderAttachment;
typedef AudioProcessorValueTreeState::ComboBoxAttachment ComboBoxAttachment;

//==============================================================================
/**
    The GUI editor for the WaveFieldSynthesizer plugin.

    The GUI consists of a title and header section, sliders as well as an XYPad
    for the source and listener position.

    This class also implements state variables together with the
    WfsAudioProcessor to update the GUI only when needed.
*/
class WfsAudioProcessorEditor  : public AudioProcessorEditor, private Timer
{
  public:
    //========= JUCE Editor Template functions =====================================
    WfsAudioProcessorEditor (WfsAudioProcessor&, AudioProcessorValueTreeState&);
    ~WfsAudioProcessorEditor() override;

    /** Called each time the GUI is constructed anew. */
    void paint (Graphics&) override;

    /** Called each time the GUI window is resized. Orders all GUI elements in the GUI. */
    void resized() override;

    /** Used to update the GUI components after state changes in PluginProcessor. */
    void timerCallback() override;

  private:
    /** The lookAndFeel class with the IEM plug-in suite design. */
    LaF globalLaF;

    /** stored references to the AudioProcessor and ValueTreeState holding all the parameters. */
    WfsAudioProcessor& audioProcessor;
    AudioProcessorValueTreeState& valueTreeState;

    /** The title and the footer component.
     Title component can hold different widgets for in- and output:
        - NoIOWidget (if there's no need for an input or output widget)
        - AudioChannelsIOWidget<maxNumberOfChannels, isChoosable>
        - AmbisonicIOWidget<maxOrder>
        - DirectivitiyIOWidget
     */
    TitleBar<AudioChannelsIOWidget<4, true>, AudioChannelsIOWidget<48, true>> title;
    OSCFooter footer;

    /** The attachments which connect IOWidget comboboxes and the associated parameters. */
    std::unique_ptr<ComboBoxAttachment> cbInputChannelsSettingAttachment;
    std::unique_ptr<ComboBoxAttachment> cbOutputChannelsSettingAttachment;

    /** Plugin GUI parameters (derived from WaveFieldSynthesis). */
    Slider slNLoudspeakers, slLoudspeakerArrayLength, slLoudspeakerRadius, slRotation, slZoom;
    std::unique_ptr<SliderAttachment> slNLoudspeakersAttachment, slLoudspeakerArrayLengthAttachment,
        slLoudspeakerArrayRadiusAttachment, slRotationAttachment, slZoomAttachment;
    ComboBox cbLoudspeakerArrangement;
    std::unique_ptr<ComboBoxAttachment> cbLoudspeakerArrangementAttachment;
    Label lbLoudspeakerArrangement, lbNLoudspeakers, lbLoudspeakerArrayLength, lbLoudspeakerRadius,
        lbRotation, lbZoom;

    /** The source and listener position pad and its corresponding sliders with attachments. */
    WfsXyPad sourcePositionPad;
    Slider slSourcePositionX, slSourcePositionY, slListenerPositionX, slListenerPositionY;
    std::unique_ptr<SliderAttachment> slSourcePositionXAttachment, slSourcePositionYAttachment,
        slListenerPositionXAttachment, slListenerPositionYAttachment;

    /** Previous values for timerCallback(). */
    LoudspeakerArrangementType previousLoudspeakerArrangementType;
    float previousZoom;

    /** GUI dimensions. */
    const int minHeightInPixels = 600;
    const float widthHeightRatio = 1.118; /* With this ratio the circle arrangement is circular. */
    const float maxSizeRatio = 1.5; /* maxHeight = minHeight * maxSizeRatio */

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WfsAudioProcessorEditor)
};
