# This CMake Template is based on the pamplejuce project (https://github.com/sudara/pamplejuce/)
# and the IEM Plug-in Suite (https://git.iem.at/audioplugins/IEMPluginSuite)

# JUCE requires a CMake Version of 3.22 or higher
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

# Function to add a git submodule through CMake, 
# seen at https://www.scivision.dev/cmake-git-submodule/ and
# https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
function(add_git_submodule dir)
# add a Git submodule directory to CMake, assuming the
# Git submodule directory is a CMake project.
#
# Usage: in CMakeLists.txt
# 
# include(AddGitSubmodule.cmake)
# add_git_submodule(mysubmod_dir)
find_package(Git REQUIRED)
if(NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${dir}
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND_ERROR_IS_FATAL ANY)

endif()
add_subdirectory(${dir})

endfunction(add_git_submodule)

# Name of the plugin
# This cannot have spaces (but PRODUCT_NAME can)
set(PROJECT_NAME "WaveFieldSyntheisizer")

# For simplicity, the name of the project is also the name of the target
project(${PROJECT_NAME} 
        VERSION 0.1.1
        LANGUAGES CXX C
        HOMEPAGE_URL "https://iem.kug.ac.at/")

# The following options let you select the plug-ins formats you want to build.
# You can either change them here or use the command line to switch them on/off.
option(IEM_BUILD_VST2 "Build VST2 version of the plug-ins." OFF)
option(IEM_BUILD_VST3 "Build VST3 version of the plug-ins." OFF)
option(IEM_BUILD_STANDALONE "Build standalones of the plug-ins." ON)
option(IEM_STANDALONE_JACK_SUPPORT "Build standalones with JACK support." OFF)
option(IEM_MACOS_UNIVERSAL "Build universal versions for macOS for Intel and Apple Silicon." OFF)

set (IEM_POST_BUILD_INSTALL "USER" CACHE STRING "Define if plug-ins are installed for USER, SYSTEM or if you want NO installation")

# By default we don't want Xcode schemes to be made for modules, etc
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# Enable to build universal binaries on macOS, increasing build time
# This only affects local builds, GitHub actions always builds Universals
if(IEM_MACOS_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Support macOS/OS X down 10.14")

# Adds all the module sources so they appear correctly in the IDE
# Must be set before JUCE is added as a sub-dir (or any targets are made)
# https://github.com/juce-framework/JUCE/commit/6b1b4cf7f6b1008db44411f2c8887d71a3348889
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Create a /Modules directory in the IDE with the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

set(VST2SDKPATH "src/VST_SDK/VST2_SDK" CACHE STRING "Path to VST2 SDK")

# Add git submodule of JUCE
add_git_submodule(JUCE)

# Set the plugin formats you'll be building.
# Valid formats: AAX Unity VST AU AUv3 LV2 Standalone
set(FORMATS "")

if(IEM_BUILD_VST2)
    message("-- IEM: Building VST2 versions")
    list(APPEND FORMATS VST)
    # checking VST2 SDK
    if(DEFINED VST2SDKPATH)
        juce_set_vst2_sdk_path (${VST2SDKPATH})
    else()
        message(FATAL_ERROR "You have to specify the VST2SDKPATH variable with the path to the VST2 SDK if you want to build VST2 versions.")
    endif()
endif()

if(IEM_BUILD_VST3)
    message("-- IEM: Building VST3 versions")
    list(APPEND FORMATS VST3)
endif()

if(IEM_BUILD_LV2)
    message("-- IEM: Building LV2 versions")
    list(APPEND FORMATS LV2)
endif()

if(IEM_BUILD_AAX)
    message("-- IEM: Building AAX versions")
    list(APPEND FORMATS AAX)
endif()

if(IEM_BUILD_STANDALONE)
    message("-- IEM: Building Standalone versions")
    list(APPEND FORMATS Standalone)
endif()

# Copy VST-Plugins into default directory
# set copy paths for built plug-ins
if (IEM_POST_BUILD_INSTALL STREQUAL "NO")
    set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD OFF)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD ON)

    if (IEM_POST_BUILD_INSTALL STREQUAL "SYSTEM")
        set (prefix "/Library/Audio/Plug-Ins")
    else()  # Defaulting to USER
        set (prefix "$ENV{HOME}/Library/Audio/Plug-Ins")
    endif()

    set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "${prefix}/VST/IEM")
    set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "${prefix}/VST3/IEM")
    set_directory_properties (PROPERTIES JUCE_LV2_COPY_DIR "${prefix}/LV2")
    set_directory_properties (PROPERTIES JUCE_AAX_COPY_DIR "/Library/Application Support/Avid/Audio/Plug-Ins/IEM")
    # TODO: I've found no references for a local AAX installation on macOS, so I need to check this

elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD ON)

    if(MSVC)
        message (STATUS "MSVC detected, setting runtime library to Multi-threaded (/MT)")
        add_compile_options($<$<CONFIG:Release>:/MT> # Runtime library: Multi-threaded
                            $<$<CONFIG:RelWithDebInfo>:/MT> # Runtime library: Multi-threaded                           
                            $<$<CONFIG:Debug>:/MTd> # Runtime library: Multi-threaded Debug
                            )
    endif()

    if ((IEM_POST_BUILD_INSTALL STREQUAL "USER") AND (prefix STREQUAL "/usr/local"))
        if (IEM_BUILD_VST3 OR IEM_BUILD_VST2 OR IEM_BUILD_AAX)
            message (WARNING "VST, VST3 and AAX plug-ins are installed for all users by default as there is no officially supported consistent directory for local installation. Using SYSTEM for those. You can provide a costum prefix to change this behaviour.")
            set (IEM_POST_BUILD_INSTALL "SYSTEM")
        endif()

        set_directory_properties(PROPERTIES JUCE_LV2_COPY_DIR "$ENV{APPDATA}/LV2")
    endif()

    if (IEM_POST_BUILD_INSTALL STREQUAL "USER")
        set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "${prefix}/VST")
        set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "${prefix}/VST3")
        set_directory_properties(PROPERTIES JUCE_LV2_COPY_DIR "${prefix}/LV2")
        set_directory_properties(PROPERTIES JUCE_AAX_COPY_DIR "${prefix}/AAX")

    else()  # Defaulting to SYSTEM as this is the only valid path on Windows for VST and VST3
        set_directory_properties(PROPERTIES JUCE_LV2_COPY_DIR "$ENV{CommonProgramFiles}/LV2")

        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{ProgramW6432}/Steinberg/VSTplugins/IEM")
            set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "$ENV{CommonProgramFiles}/VST3/IEM")
            set_directory_properties (PROPERTIES JUCE_AAX_COPY_DIR "$ENV{CommonProgramFiles}/Avid/Audio/Plug-Ins/IEM")
        else()
            set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{ProgramFiles(x86)}/Steinberg/Vstplugins/IEM")
            set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "$ENV{CommonProgramFiles(x86)}/VST3/IEM")
        endif()

    endif()

elseif ((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (CMAKE_SYSTEM_NAME MATCHES ".*BSD"))
    set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD ON)

    if (IEM_POST_BUILD_INSTALL STREQUAL "SYSTEM")
        set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "${prefix}/lib/vst/IEM")
        set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "${prefix}/lib/vst3/IEM")
        set_directory_properties (PROPERTIES JUCE_LV2_COPY_DIR "${prefix}/lib/lv2")
    else()  # Defaulting to USER
        set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{HOME}/.vst/IEM")
        set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3/IEM")
        set_directory_properties (PROPERTIES JUCE_LV2_COPY_DIR "$ENV{HOME}/.lv2")
    endif()
endif()

# Check the readme at `docs/CMake API.md` in the JUCE repo for full config
juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG resources/Standalone/IEMPluginSuiteSmall.png
    ICON_SMALL resources/Standalone/IEMPluginSuiteSmall.png
    COMPANY_NAME "IEM"
    PLUGIN_MANUFACTURER_CODE Iema               # This has to be one uppercase, rest lower for AU formats
    PLUGIN_CODE WFSy                            # A unique four-character plugin id with at least one upper-case character
    FORMATS "${FORMATS}"
    VERSION "${VERSION}"
    PRODUCT_NAME "${PROJECT_NAME}"              # This is the name of the plugin as it appears in the host
    LV2URI "http://plugins.iem.at/WaveFieldSyntheisizer"
    VST2_CATEGORY kPlugSurroundFx kPlugCategSpacializer
    VST3_CATEGORIES Spatial Surround
    AAX_CATEGORY SoundField
)        

include_directories(resources/ /usr/local/include)
add_compile_definitions(DONT_SET_USING_JUCE_NAMESPACE=1
                        JUCE_MODAL_LOOPS_PERMITTED=1)

juce_add_binary_data(LAF_fonts SOURCES
    resources/lookAndFeel/Roboto-Bold.ttf
    resources/lookAndFeel/Roboto-Light.ttf
    resources/lookAndFeel/Roboto-Medium.ttf
    resources/lookAndFeel/Roboto-Regular.ttf)

juce_add_binary_data (IEM_logo 
    HEADER_NAME "IEMLogo.h"
    NAMESPACE "IEMLogo"
    SOURCES
    resources/Standalone/IEMPluginSuiteSmall.png)


juce_generate_juce_header("${PROJECT_NAME}")

# Manually list all .h and .cpp files for the plugin
set(SourceFiles
    Source/PluginEditor.h
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp

    Source/CustomUtilities.h
    Source/WfsSpeakerProcessor.h
    Source/WfsSpeakerProcessor.cpp
    Source/WfsXyPad.h
    Source/WfsXyPadBackground.h)

set(ResourceFiles    
    resources/OSC/OSCInputStream.h
    resources/OSC/OSCParameterInterface.cpp
    resources/OSC/OSCParameterInterface.h
    resources/OSC/OSCStatus.cpp
    resources/OSC/OSCStatus.h
    resources/OSC/OSCUtilities.h)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles} ${ResourceFiles})

# Those are only needed for standalone app support
if(Standalone IN_LIST FORMATS)
    set(StandaloneFiles
        resources/Standalone/StandaloneApp.cpp
        resources/Standalone/MyStandaloneFilterWindow.h
        resources/Standalone/IEM_JackAudio.h
        resources/Standalone/IEM_AudioDeviceSelectorComponent.cpp
        resources/Standalone/IEM_AudioDeviceSelectorComponent.h)
    target_sources("${PROJECT_NAME}" PRIVATE ${StandaloneFiles})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${StandaloneFiles})
endif()

# Disable Jack support if it is not desired
if (NOT IEM_STANDALONE_JACK_SUPPORT)
    target_compile_definitions("${PROJECT_NAME}" PUBLIC DONT_BUILD_WITH_JACK_SUPPORT=1)
endif()

# No, we don't want our source buried in extra nested folders
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# The Xcode source tree should uhhh, still look like the source tree, yo
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${ResourceFiles})

# Required for Linux happiness:
# See https://forum.juce.com/t/loading-pytorch-model-using-binarydata/39997/2
set_target_properties(LAF_fonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# This cleans up the folder organization on Xcode.
# It tucks the Plugin varieties into a "Targets" folder and generate an Xcode Scheme manually
# Xcode scheme generation is turned off globally to limit noise from other targets
# The non-hacky way of doing this is via the global PREDEFINED_TARGETS_FOLDER property
# However that doesn't seem to be working in Xcode
# Not all plugin types (au, vst) available on each build type (win, macos, linux)
foreach(target ${FORMATS} "All")
    if(TARGET ${PROJECT_NAME}_${target})
        set_target_properties(${PROJECT_NAME}_${target} PROPERTIES
            # Tuck the actual plugin targets into a folder where they won't bother us
            FOLDER "Targets"

            # Let us build the target in Xcode
            XCODE_GENERATE_SCHEME ON)
    endif()
endforeach()
set_target_properties(LAF_fonts PROPERTIES FOLDER "Targets")

# We'll need to link to these from our plugin as well as our tests
set(JUCE_DEPENDENCIES
    juce::juce_audio_utils)


target_compile_definitions("${PROJECT_NAME}" PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_include_directories("${PROJECT_NAME}" PRIVATE Source)

target_link_libraries("${PROJECT_NAME}" PRIVATE
    LAF_fonts
    IEM_logo
    ${JUCE_DEPENDENCIES}
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_osc
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Color our warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()
    