cmake_minimum_required(VERSION 3.22)

project(KeyDetector VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Locate JUCE.
#   * By default we use a sibling JUCE checkout (../JUCE) via add_subdirectory.
#   * If it isn't there (or KEYDETECTOR_FETCH_JUCE=ON) we download a pinned
#     release with FetchContent so the project builds from a clean clone.
# ==============================================================================
option(KEYDETECTOR_FETCH_JUCE "Download JUCE via FetchContent instead of a local copy" OFF)
option(KEYDETECTOR_BUILD_TESTS "Build the standalone DSP unit test (no JUCE needed)"    ON)

if(NOT KEYDETECTOR_FETCH_JUCE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    message(STATUS "KeyDetector: using local JUCE at ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE")
else()
    message(STATUS "KeyDetector: fetching JUCE 8.0.9 via FetchContent")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ==============================================================================
# The plugin target.
#
# IS_SYNTH is FALSE: a key detector must *receive* audio to analyse, so it is an
# analyser/effect (it appears on an audio track's insert chain and passes audio
# through unchanged).  A sound-generating "instrument" has no audio input, which
# is the opposite of what this plugin needs.  Flip IS_SYNTH/NEEDS_MIDI_INPUT if
# you specifically need it to register as a VSTi in your host.
# ==============================================================================
juce_add_plugin(KeyDetector
    PLUGIN_MANUFACTURER_CODE    Dspx
    PLUGIN_CODE                 Kdet
    COMPANY_NAME                "DSPExample"
    BUNDLE_ID                   com.dspexample.keydetector
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Analyzer" "Fx"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Key Detector")

target_sources(KeyDetector PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpectrumAnalyzer.cpp
    Source/ChromaKeyDetector.cpp
    Source/PitchDetector.cpp
    Source/TempoEstimator.cpp)

target_compile_definitions(KeyDetector PUBLIC
    JUCE_WEB_BROWSER=0            # avoids the webkit/gtk dependency
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KeyDetector
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ==============================================================================
# Standalone DSP unit test — pure C++17, links no JUCE, so it always builds and
# runs quickly (ctest).
# ==============================================================================
if(KEYDETECTOR_BUILD_TESTS)
    enable_testing()
    add_executable(ChromaKeyDetectorTest
        tests/ChromaKeyDetectorTest.cpp
        Source/ChromaKeyDetector.cpp)
    target_include_directories(ChromaKeyDetectorTest PRIVATE Source)
    add_test(NAME ChromaKeyDetectorTest COMMAND ChromaKeyDetectorTest)

    add_executable(PitchDetectorTest
        tests/PitchDetectorTest.cpp
        Source/PitchDetector.cpp)
    target_include_directories(PitchDetectorTest PRIVATE Source)
    add_test(NAME PitchDetectorTest COMMAND PitchDetectorTest)

    add_executable(TempoEstimatorTest
        tests/TempoEstimatorTest.cpp
        Source/TempoEstimator.cpp)
    target_include_directories(TempoEstimatorTest PRIVATE Source)
    add_test(NAME TempoEstimatorTest COMMAND TempoEstimatorTest)
endif()
