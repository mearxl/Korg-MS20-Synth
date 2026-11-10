cmake_minimum_required(VERSION 3.22)

project(KorgMS20 VERSION 1.0.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")

add_subdirectory(external/JUCE)

juce_add_plugin(KorgMS20
    COMPANY_NAME "KeioElectronicLab"
    BUNDLE_ID "com.keio.korgms20"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Keio
    PLUGIN_CODE Ms20
    FORMATS AU AUv3 VST3 Standalone
    PRODUCT_NAME "Korg MS-20"
)

target_sources(KorgMS20
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/Korg35Filter.h
        Source/DSP/MS20Oscillators.h
        Source/DSP/MS20Modulators.h
        Source/DSP/MS20ESP.h
        Source/DSP/MS20PatchBay.h
        Source/DSP/MS20SynthEngine.h
)

target_compile_definitions(KorgMS20
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KorgMS20
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_executable(MS20VerifyDSP Source/VerifyDSP.cpp)
target_include_directories(MS20VerifyDSP PRIVATE Source)

