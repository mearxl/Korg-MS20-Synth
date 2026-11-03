#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

enum class MS20KnobStyle
{
    Selector4,       // VCO1/2 Wave & Scale 4-position rotary switches (-54 deg to +54 deg)
    LargeContinuous, // HPF Cutoff, LPF Cutoff, Master Volume (0..10 numbered)
    SmallUnipolar,   // Standard 0..10 numbered small fluted knob
    SmallBipolar,    // -5..0..+5 numbered small fluted knob (Pitch, Master Tune)
    SmallPW,         // VCO 1 PW knob with square-to-pulse icons
    SmallMGWave      // MG Waveform knob with ramp/tri/saw & pulse icons
};

class MS20LookAndFeel : public juce::LookAndFeel_V4
{
public:
    MS20LookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;
};

/**
 * Custom vertical ribbed MS-20 Control Wheel + Momentary Push Switch component
 * located on the left cheek beside the 37-key keyboard.
 */
class MS20WheelDeck : public juce::Component
{
public:
    explicit MS20WheelDeck(MS20AudioProcessor& p);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    MS20AudioProcessor& processor;
    bool draggingWheel = false;
    bool pressingMomentary = false;
    float dragStartY = 0.0f;
    float dragStartWheelVal = 0.0f;

    juce::Rectangle<float> getWheelBounds() const noexcept;
    juce::Rectangle<float> getButtonBounds() const noexcept;
};

class MS20AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit MS20AudioProcessorEditor(MS20AudioProcessor&);
    ~MS20AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    MS20AudioProcessor& processorRef;
    MS20LookAndFeel ms20LookAndFeel;

    struct KnobEntry
    {
        juce::String paramId;
        juce::String bottomLabel;
        MS20KnobStyle style = MS20KnobStyle::SmallUnipolar;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Point<float> center;
        int diameter = 58;
    };

    std::vector<KnobEntry> knobs;
    void addKnob(const juce::String& paramId, const juce::String& bottomLabel, MS20KnobStyle style);
    KnobEntry* findKnob(const juce::String& paramId);

    // Minimal top utility strip
    juce::ComboBox presetBox;
    juce::TextButton clearPatchBtn { "UNPLUG CABLES" };
    juce::TextButton holdDroneBtn  { "KEY HOLD" };
    bool droneActive = false;

    // Left-hand Wheel + Momentary Switch deck
    MS20WheelDeck wheelDeck;

    // Bottom 37-key Keyboard
    juce::MidiKeyboardComponent midiKeyboard;

    // 35-Jack Patch Bay layout matching KLM-128C / KLM-129 photo
    struct JackVisual
    {
        bool isSource = true;
        int index = 0;
        juce::String topLabel;
        juce::String bottomLabel;
        juce::Point<float> center;
        bool hasGndSymbol = false;
    };

    std::vector<JackVisual> jacks;
    void layoutPatchJacks();
    int hitTestJack(juce::Point<float> pt, float maxDist = 15.0f) const;

    // Interactive cable dragging state
    bool isDraggingCable = false;
    int dragStartJackIdx = -1;
    juce::Point<float> dragCurrentPt;
    int hoveredJackIdx = -1;

    void drawLeftControlPanel(juce::Graphics& g) const;
    void drawRightPatchPanel(juce::Graphics& g) const;
    void drawSelectorScaleLabels(juce::Graphics& g, const KnobEntry& k) const;
    void drawPatchJack(juce::Graphics& g, const JackVisual& jack, bool isHovered, bool isConnected) const;
    void drawCatenaryCable(juce::Graphics& g, juce::Point<float> p1, juce::Point<float> p2, juce::Colour col, float alpha = 0.90f) const;
    void drawLed(juce::Graphics& g, juce::Point<float> center, bool on) const;
    void drawScrew(juce::Graphics& g, float cx, float cy, float r = 5.0f, bool isRivet = false) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MS20AudioProcessorEditor)
};
