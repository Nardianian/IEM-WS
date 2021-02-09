#  WaveFieldSynthesizer 

## Overview

This audio plugin implements a wave-field synthesis for an arbitrary loudspeaker array. The wave-field synthesis parameters are controllable via GUI and via automation. The plugin is free and open-source.

## Demovideo

[![Demonstrational video](Misc/WaveFieldSynthesizer_20210209.png)](https://www.youtube.com/watch?v=57Pwy940GIA&feature=youtu.be "WaveFieldSynthesizer")

## Installation Guide

You don't need to install the plugin. Simply move the VST file to your local VST plugin folder.

- Windows: C:\Program Files\Common Files\VST3 or C:\Programm Files\Steinberg\VstPlugins 
- MacOS: /Library/Audio/Plug-Ins/Components or /Library/Audio/Plug-Ins/VST
- Linux: ~/.vst3 or /usr/local/lib/lxvst

After adding the plugin you need to restart your DAW, and occasionally re-scan for new VSTs.

## Compilation Guide

For compiling the plugin you need the [JUCE framework](https://juce.com), a C++ environment and an IDE. 
For Windows you need Visual Studio (with C++ tools installed), for MacOS you need XCode). For Linux it is enough to use Unix Makefile support.

To generate the build files, you have to open the [WaveFieldSynthesizer.jucer](WaveFieldSynthesizer.jucer) file and save the project. This generates the build files for all operating systems in the *Build/* directory.

The project builds a VST version as well as a Standalone version. Usually you will use the VST version in your favourite DAW. For simple testing the Standalone is useful though.

### Step-by-step

- Clone or download this repository
- Open the [WaveFieldSynthesizer.jucer](WaveFieldSynthesizer.jucer] file 
- Export the build files by saving the project ("File" -> "Save")
- **For Windows:** Open the VisualStudio2017/2019 solution in *Build/* and build the VST3 target
- **For MacOS:** Open the XCode file in *Build/* and build the VST3 target
- **For Linux:** Open a terminal, navigate into the Linux Makefile directory in *Build/*, then "make"

### JACK support

The Standalone version is built with JACK support on MacOS and Linux. You can also choose to disable the JACK support by adding `DONT_BUILD_WITH_JACK_SUPPORT=1` to the Preprocessor Definitions in the Projucer project.

## Binaural listening

Wave-field synthesis usually uses a lot of loudspeakers, which are normally not available in home setups. It's possible to use the plugin for binaural listening too. You need the following signal chain:

- Audio track (preferably Mono, more channels possible)
- WaveFieldSynthesizer
- Binaural rendering plugin with the corresponding loudspeakers-to-binaural impulse responses

### Binaural rendering plugin

One possible plugin is Matthias Kronlachner's [https://github.com/kronihias/mcfx](mcfx_convolver) (binaries available [here](http://www.matthiaskronlachner.com/?p=1910)). You can use any binaural rendering plugin though.

Additionally you need impulse responses. You can find some for Binaural and for Ambisonics 1st order [under this link](http://soundaroundcloud.ddns.net:8000/index.php/s/JHfxkgpoxw8k5G5). The impulse responses were recorded using a linear 24 loudspeaker array and 5 different listener positions.

Additionally you may need a configuration file for the rendering plugin. In the [Misc directory](Misc/BinauralRoomImpulseResponsesPosition3.conf) you find one for binaural listening, recorded at the central listener position. Use it together with the *mcfx_convolver*.

## Possible issues

If you build on Windows, a file path in the build process may become too long. Visual Studio then throws an error. If this happens, try moving the repository to a shorter path, e.g. *C:/*.

## Related work

For a comprehensive guide on modern wave field synthesis, see [Gergely Firtha's PhD thesis](https://repozitorium.omikk.bme.hu/bitstream/handle/10890/13177/tezis_eng.pdf?sequence=3&isAllowed=y).
