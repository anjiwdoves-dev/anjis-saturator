cmake_minimum_required(VERSION 3.22)
project(AnjisSaturator VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so the plugin loads on any Windows machine without extra DLLs.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(ANJIS_FORMATS VST3 AU Standalone)
else()
    set(ANJIS_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AnjisSaturator
    COMPANY_NAME                "Anji"
    BUNDLE_ID                   com.anji.saturator
    PLUGIN_MANUFACTURER_CODE    Anji
    PLUGIN_CODE                 AnSt
    FORMATS                     ${ANJIS_FORMATS}
    PRODUCT_NAME                "Anjis Saturator"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect)

juce_generate_juce_header(AnjisSaturator)

juce_add_binary_data(AnjisData SOURCES
    Resources/fonts/Poppins-Medium.ttf
    Resources/fonts/Poppins-Bold.ttf
    Resources/fonts/Inter-Medium.otf
    Resources/fonts/Inter-SemiBold.otf)

target_sources(AnjisSaturator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp)

target_compile_definitions(AnjisSaturator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AnjisSaturator
    PRIVATE
        AnjisData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Offline DSP test (developer only; not needed for the plugin build)
option(ANJIS_BUILD_TESTS "Build offline DSP tests" OFF)
if(ANJIS_BUILD_TESTS)
    juce_add_console_app(AnjisTest PRODUCT_NAME "AnjisTest")
    target_sources(AnjisTest PRIVATE Tests/OfflineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_include_directories(AnjisTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/AnjisSaturator_artefacts/JuceLibraryCode)
    target_compile_definitions(AnjisTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Anjis" JucePlugin_Build_Standalone=0)
    target_link_libraries(AnjisTest PRIVATE AnjisData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
