#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/MS20SynthEngine.h"

class MS20AudioProcessor : public juce::AudioProcessor
{
public:
    MS20AudioProcessor();
    ~MS20AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    ms20::dsp::MS20SynthEngine& getSynthEngine() noexcept { return synthEngine; }
    const ms20::dsp::MS20SynthEngine& getSynthEngine() const noexcept { return synthEngine; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> params;
        std::vector<std::pair<ms20::dsp::PatchSource, ms20::dsp::PatchDest>> cables;
    };

    const std::vector<FactoryPreset>& getFactoryPresets() const noexcept { return factoryPresets; }
    void loadFactoryPreset(int index);

private:
    juce::AudioProcessorValueTreeState apvts;
    ms20::dsp::MS20SynthEngine synthEngine;
    juce::MidiKeyboardState keyboardState;
    std::vector<FactoryPreset> factoryPresets;
    int currentPresetIndex = 0;

    void initFactoryPresets();
    ms20::dsp::MS20Params readCurrentParams() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MS20AudioProcessor)
};
