#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
constexpr float kHalfPi = juce::MathConstants<float>::halfPi;

void drawArrowLine(juce::Graphics& g, float x1, float y1, float x2, float y2, float headSize = 4.5f)
{
    g.drawLine(x1, y1, x2, y2, 1.15f);
    const float angle = std::atan2(y2 - y1, x2 - x1);
    juce::Path head;
    head.startNewSubPath(x2, y2);
    head.lineTo(x2 - headSize * std::cos(angle - 0.45f), y2 - headSize * std::sin(angle - 0.45f));
    head.lineTo(x2 - headSize * std::cos(angle + 0.45f), y2 - headSize * std::sin(angle + 0.45f));
    head.closeSubPath();
    g.fillPath(head);
}

void drawWaveformIcon(juce::Graphics& g, const juce::String& type, float cx, float cy, float w, float h)
{
    juce::Path p;
    const float x0 = cx - w * 0.5f;
    const float x1 = cx + w * 0.5f;
    const float yTop = cy - h * 0.5f;
    const float yBot = cy + h * 0.5f;

    if (type == "tri")
    {
        p.startNewSubPath(x0, yBot);
        p.lineTo(cx, yTop);
        p.lineTo(x1, yBot);
    }
    else if (type == "saw")
    {
        p.startNewSubPath(x0, yTop);
        p.lineTo(x0, yBot);
        p.lineTo(x1, yTop);
        p.lineTo(x1, yBot);
    }
    else if (type == "sq")
    {
        p.startNewSubPath(x0, yBot);
        p.lineTo(x0, yTop);
        p.lineTo(cx, yTop);
        p.lineTo(cx, yBot);
        p.lineTo(x1, yBot);
    }
    else if (type == "narrow")
    {
        const float xm = x0 + w * 0.26f;
        p.startNewSubPath(x0, yBot);
        p.lineTo(x0, yTop);
        p.lineTo(xm, yTop);
        p.lineTo(xm, yBot);
        p.lineTo(x1, yBot);
    }
    else if (type == "pw")
    {
        // Square <-> Narrow Pulse symbol
        p.startNewSubPath(x0, yBot);
        p.lineTo(x0, yTop);
        p.lineTo(x0 + w * 0.32f, yTop);
        p.lineTo(x0 + w * 0.32f, yBot);
        p.lineTo(x1, yBot);
    }
    else if (type == "noise")
    {
        p.startNewSubPath(x0, cy);
        p.lineTo(x0 + w * 0.16f, yTop);
        p.lineTo(x0 + w * 0.32f, yBot);
        p.lineTo(x0 + w * 0.48f, yTop + 1.5f);
        p.lineTo(x0 + w * 0.64f, yBot - 1.0f);
        p.lineTo(x0 + w * 0.82f, yTop);
        p.lineTo(x1, cy);
    }
    else if (type == "ramp")
    {
        p.startNewSubPath(x0, yBot);
        p.lineTo(x1, yTop);
        p.lineTo(x1, yBot);
    }
    g.strokePath(p, juce::PathStrokeType(1.2f));
}
} // namespace

//==============================================================================
MS20LookAndFeel::MS20LookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1A1C1E));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xFFEAEAE4));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF45484C));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF202226));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFE8E8E2));
}

void MS20LookAndFeel::drawRotarySlider(juce::Graphics& g,
                                       int x, int y, int width, int height,
                                       float sliderPosProportional,
                                       float rotaryStartAngle,
                                       float rotaryEndAngle,
                                       juce::Slider& slider)
{
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const int styleInt = slider.getProperties().getWithDefault("ms20Style", static_cast<int>(MS20KnobStyle::SmallUnipolar));
    const auto style = static_cast<MS20KnobStyle>(styleInt);

    const float radius = std::min(static_cast<float>(width), static_cast<float>(height)) * 0.295f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle) - kHalfPi;

    // 1. Draw Scale Ticks & Numbers around the knob
    g.setColour(juce::Colour(0xFFEAEAE2));
    if (style == MS20KnobStyle::Selector4)
    {
        // 4 stepped radial ticks (-54, -18, +18, +54 deg from top)
        for (int i = 0; i < 4; ++i)
        {
            const float t = static_cast<float>(i) / 3.0f;
            const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - kHalfPi;
            const float r1 = radius * 1.04f;
            const float r2 = radius * 1.32f;
            g.drawLine(cx + std::cos(a) * r1, cy + std::sin(a) * r1,
                       cx + std::cos(a) * r2, cy + std::sin(a) * r2, 1.3f);
        }
    }
    else
    {
        // 11 main ticks (0..10 or -5..+5)
        constexpr int numTicks = 11;
        g.setFont(juce::FontOptions(style == MS20KnobStyle::LargeContinuous ? 9.5f : 8.2f, juce::Font::bold));
        for (int i = 0; i < numTicks; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - kHalfPi;
            const float r1 = radius * 1.05f;
            const float r2 = radius * 1.24f;
            g.drawLine(cx + std::cos(a) * r1, cy + std::sin(a) * r1,
                       cx + std::cos(a) * r2, cy + std::sin(a) * r2, 1.15f);

            if (style == MS20KnobStyle::LargeContinuous || style == MS20KnobStyle::SmallUnipolar || style == MS20KnobStyle::SmallBipolar)
            {
                const float rText = radius * 1.44f;
                const float tx = cx + std::cos(a) * rText;
                const float ty = cy + std::sin(a) * rText;
                juce::String numStr;
                if (style == MS20KnobStyle::SmallBipolar)
                {
                    const int v = i - 5;
                    numStr = (v > 0) ? ("+" + juce::String(v)) : juce::String(v);
                }
                else
                {
                    numStr = juce::String(i);
                }
                g.drawText(numStr, static_cast<int>(tx - 11.0f), static_cast<int>(ty - 6.0f),
                           22, 12, juce::Justification::centred, false);
            }
        }

        if (style == MS20KnobStyle::SmallPW)
        {
            drawWaveformIcon(g, "sq",     cx - radius * 1.15f, cy + radius * 1.05f, 8.0f, 5.5f);
            drawWaveformIcon(g, "sq",     cx,                  cy - radius * 1.40f, 8.0f, 5.5f);
            drawWaveformIcon(g, "narrow", cx + radius * 1.15f, cy + radius * 1.05f, 8.0f, 5.5f);
        }
        else if (style == MS20KnobStyle::SmallMGWave)
        {
            drawWaveformIcon(g, "saw",    cx - radius * 1.15f, cy + radius * 1.08f, 8.0f, 5.5f);
            drawWaveformIcon(g, "tri",    cx,                  cy - radius * 1.38f, 8.0f, 5.5f);
            drawWaveformIcon(g, "ramp",   cx + radius * 1.15f, cy + radius * 1.08f, 8.0f, 5.5f);
        }
    }

    // 2. Knob Drop Shadow (angled down-right like the studio photograph)
    g.setColour(juce::Colour(0xAA000000));
    g.fillEllipse(cx - radius * 0.96f, cy - radius * 0.88f, radius * 2.08f, radius * 2.08f);

    if (style == MS20KnobStyle::Selector4)
    {
        // Smooth skirted rotary selector knob with wide raised pointer bar (see VCO 1/2 WAVE FORM & SCALE in photo)
        juce::ColourGradient baseGrad(juce::Colour(0xFF3C3F44), cx - radius * 0.5f, cy - radius * 0.6f,
                                      juce::Colour(0xFF15171A), cx + radius * 0.6f, cy + radius * 0.6f, false);
        g.setGradientFill(baseGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(juce::Colour(0xFF52565C));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        const float innerR = radius * 0.82f;
        juce::ColourGradient capGrad(juce::Colour(0xFF34373C), cx - innerR * 0.5f, cy - innerR * 0.5f,
                                     juce::Colour(0xFF1A1C20), cx + innerR * 0.5f, cy + innerR * 0.5f, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

        // Raised center selector bar rotated to `angle`
        juce::Path bar;
        const float barHalfW = radius * 0.19f;
        bar.addRoundedRectangle(- innerR * 0.95f, -barHalfW, innerR * 1.90f, barHalfW * 2.0f, 2.5f);
        bar.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
        g.setColour(juce::Colour(0xFF22252A));
        g.fillPath(bar);
        g.setColour(juce::Colour(0xFF494D54));
        g.strokePath(bar, juce::PathStrokeType(1.0f));

        // White pointer line on the upper half of the bar
        g.setColour(juce::Colour(0xFFF4F4EE));
        g.drawLine(cx + std::cos(angle) * (radius * 0.22f),
                   cy + std::sin(angle) * (radius * 0.22f),
                   cx + std::cos(angle) * (radius * 0.94f),
                   cy + std::sin(angle) * (radius * 0.94f), 2.3f);
    }
    else
    {
        // Fluted MS-20 continuous knob (Large or Small)
        juce::ColourGradient skirtGrad(juce::Colour(0xFF383B40), cx - radius * 0.5f, cy - radius * 0.5f,
                                       juce::Colour(0xFF141619), cx + radius * 0.6f, cy + radius * 0.6f, false);
        g.setGradientFill(skirtGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(juce::Colour(0xFF4E5258));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // Scalloped grip flutes around outer skirt
        g.setColour(juce::Colour(0xFF0E1012));
        const int numFlutes = (style == MS20KnobStyle::LargeContinuous) ? 12 : 9;
        for (int i = 0; i < numFlutes; ++i)
        {
            const float a = angle + static_cast<float>(i) * (kTwoPi / static_cast<float>(numFlutes));
            const float fx = cx + std::cos(a) * (radius * 0.80f);
            const float fy = cy + std::sin(a) * (radius * 0.80f);
            g.fillEllipse(fx - radius * 0.10f, fy - radius * 0.10f, radius * 0.20f, radius * 0.20f);
        }

        // Raised top cap
        const float innerR = radius * 0.74f;
        juce::ColourGradient topGrad(juce::Colour(0xFF32353A), cx - innerR * 0.4f, cy - innerR * 0.5f,
                                     juce::Colour(0xFF181A1E), cx + innerR * 0.5f, cy + innerR * 0.5f, false);
        g.setGradientFill(topGrad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

        g.setColour(juce::Colour(0xFF454950));
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.9f);

        // Crisp white pointer line
        g.setColour(juce::Colour(0xFFF4F4EE));
        g.drawLine(cx + std::cos(angle) * (innerR * 0.15f),
                   cy + std::sin(angle) * (innerR * 0.15f),
                   cx + std::cos(angle) * (radius * 0.97f),
                   cy + std::sin(angle) * (radius * 0.97f),
                   (style == MS20KnobStyle::LargeContinuous) ? 2.4f : 2.0f);
    }
}

//==============================================================================
MS20WheelDeck::MS20WheelDeck(MS20AudioProcessor& p)
    : processor(p)
{
}

juce::Rectangle<float> MS20WheelDeck::getWheelBounds() const noexcept
{
    return { 36.0f, 22.0f, 36.0f, 102.0f };
}

juce::Rectangle<float> MS20WheelDeck::getButtonBounds() const noexcept
{
    return { 38.0f, 138.0f, 32.0f, 32.0f };
}

void MS20WheelDeck::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Angled left cheek deck surface matching the photo
    juce::ColourGradient deckGrad(juce::Colour(0xFF23262A), 0.0f, 0.0f,
                                  juce::Colour(0xFF121417), 0.0f, b.getHeight(), false);
    g.setGradientFill(deckGrad);
    g.fillRoundedRectangle(b, 3.0f);

    g.setColour(juce::Colour(0xFF3E4248));
    g.drawRoundedRectangle(b.reduced(1.0f), 3.0f, 1.2f);

    // 1. Vertical Ribbed Control Wheel
    const auto wb = getWheelBounds();
    g.setColour(juce::Colour(0xFF08090B));
    g.fillRoundedRectangle(wb.expanded(4.0f, 4.0f), 4.0f);

    juce::ColourGradient wheelGrad(juce::Colour(0xFF15171A), wb.getX(), wb.getY(),
                                   juce::Colour(0xFF15171A), wb.getX(), wb.getBottom(), false);
    wheelGrad.addColour(0.5, juce::Colour(0xFF3E4248));
    g.setGradientFill(wheelGrad);
    g.fillRoundedRectangle(wb, 3.0f);

    // Horizontal ribs on the wheel
    g.setColour(juce::Colour(0xFF111315));
    for (float ry = wb.getY() + 5.0f; ry < wb.getBottom() - 4.0f; ry += 5.5f)
        g.drawHorizontalLine(static_cast<int>(ry), wb.getX() + 2.0f, wb.getRight() - 2.0f);

    // White center index notch at current wheel position (-1..+1)
    const float wheelVal = processor.getAPVTS().getRawParameterValue("control_wheel")->load();
    const float indicatorY = wb.getCentreY() - wheelVal * (wb.getHeight() * 0.40f);
    g.setColour(juce::Colour(0xFFF5F5EE));
    g.fillRect(wb.getX() + 2.0f, indicatorY - 2.0f, wb.getWidth() - 4.0f, 4.0f);

    // Center detent tick mark on bezel
    g.setColour(juce::Colour(0xFFDCDCD4));
    g.drawLine(wb.getX() - 8.0f, wb.getCentreY(), wb.getX() - 3.0f, wb.getCentreY(), 1.5f);

    // 2. Chrome Momentary Push Button below the wheel
    const auto mb = getButtonBounds();
    const bool swOn = processor.getAPVTS().getRawParameterValue("momentary_sw")->load() > 0.5f;

    g.setColour(juce::Colour(0xFF0A0B0D));
    g.fillEllipse(mb.expanded(2.0f));

    // Chrome bezel ring
    g.setColour(juce::Colour(0xFFBDC3C7));
    g.fillEllipse(mb);
    g.setColour(juce::Colour(0xFF2C3E50));
    g.drawEllipse(mb, 1.2f);

    // Inner momentary button cap
    const auto innerBtn = mb.reduced(5.5f);
    g.setColour(swOn ? juce::Colour(0xFFE74C3C) : juce::Colour(0xFF282B30));
    g.fillEllipse(innerBtn);
    g.setColour(juce::Colour(0xFF7F8C8D));
    g.drawEllipse(innerBtn, 1.0f);
}

void MS20WheelDeck::mouseDown(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    if (getWheelBounds().expanded(8.0f).contains(pt))
    {
        draggingWheel = true;
        dragStartY = pt.y;
        dragStartWheelVal = processor.getAPVTS().getRawParameterValue("control_wheel")->load();
    }
    else if (getButtonBounds().expanded(6.0f).contains(pt))
    {
        pressingMomentary = true;
        if (auto* p = processor.getAPVTS().getParameter("momentary_sw"))
            p->setValueNotifyingHost(1.0f);
        repaint();
    }
}

void MS20WheelDeck::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingWheel)
    {
        const float dy = dragStartY - e.position.y;
        const float newVal = std::clamp(dragStartWheelVal + dy / 50.0f, -1.0f, 1.0f);
        if (auto* p = processor.getAPVTS().getParameter("control_wheel"))
            p->setValueNotifyingHost(p->convertTo0to1(newVal));
        repaint();
    }
}

void MS20WheelDeck::mouseUp(const juce::MouseEvent&)
{
    draggingWheel = false;
    if (pressingMomentary)
    {
        pressingMomentary = false;
        if (auto* p = processor.getAPVTS().getParameter("momentary_sw"))
            p->setValueNotifyingHost(0.0f);
        repaint();
    }
}

void MS20WheelDeck::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (getWheelBounds().expanded(8.0f).contains(e.position))
    {
        if (auto* p = processor.getAPVTS().getParameter("control_wheel"))
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        repaint();
    }
}

//==============================================================================
MS20AudioProcessorEditor::MS20AudioProcessorEditor(MS20AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      wheelDeck(p),
      midiKeyboard(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&ms20LookAndFeel);

    // Minimal top preset strip
    const auto& presets = processorRef.getFactoryPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        presetBox.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetBox.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]()
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
        {
            processorRef.loadFactoryPreset(idx);
            repaint();
        }
    };
    addAndMakeVisible(presetBox);

    clearPatchBtn.onClick = [this]()
    {
        processorRef.getSynthEngine().getPatchBay().clearAll();
        repaint();
    };
    addAndMakeVisible(clearPatchBtn);

    holdDroneBtn.setClickingTogglesState(true);
    holdDroneBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFC62828));
    holdDroneBtn.onClick = [this]()
    {
        droneActive = holdDroneBtn.getToggleState();
        if (droneActive)
            processorRef.getKeyboardState().noteOn(1, 48, 0.9f);
        else
            processorRef.getKeyboardState().allNotesOff(1);
    };
    addAndMakeVisible(holdDroneBtn);

    // --- Add All 33 Front-Panel Knobs with their exact hardware styles & labels ---
    // Column 1: VCO 1 & Portamento
    addKnob("vco1_wave",   "WAVE FORM", MS20KnobStyle::Selector4);
    addKnob("vco1_pw",     "PW",        MS20KnobStyle::SmallPW);
    addKnob("vco1_scale",  "SCALE",     MS20KnobStyle::Selector4);
    addKnob("portamento",  "TIME",      MS20KnobStyle::SmallUnipolar);

    // Column 2: VCO 2 & Master Tune
    addKnob("vco2_wave",   "WAVE FORM", MS20KnobStyle::Selector4);
    addKnob("vco2_pitch",  "PITCH",     MS20KnobStyle::SmallBipolar);
    addKnob("vco2_scale",  "SCALE",     MS20KnobStyle::Selector4);
    addKnob("master_tune", "",          MS20KnobStyle::SmallBipolar);

    // Column 3: VCO Mixer & Frequency Modulation
    addKnob("vco1_level",  "VCO 1 LEVEL", MS20KnobStyle::SmallUnipolar);
    addKnob("vco2_level",  "VCO 2 LEVEL", MS20KnobStyle::SmallUnipolar);
    addKnob("fm_mg_ext",   "MG/T.EXT",    MS20KnobStyle::SmallUnipolar);
    addKnob("fm_eg1_ext",  "EG1/EXT",     MS20KnobStyle::SmallUnipolar);

    // Column 4: VC HPF & Cutoff Freq Modulation
    addKnob("hpf_cutoff",  "CUTOFF FREQUENCY", MS20KnobStyle::LargeContinuous);
    addKnob("hpf_peak",    "PEAK",             MS20KnobStyle::SmallUnipolar);
    addKnob("hpf_mg_ext",  "MG/T.EXT",         MS20KnobStyle::SmallUnipolar);
    addKnob("hpf_eg2_ext", "EG2/EXT",          MS20KnobStyle::SmallUnipolar);

    // Column 5: VC LPF & Cutoff Freq Modulation
    addKnob("lpf_cutoff",  "CUTOFF FREQUENCY", MS20KnobStyle::LargeContinuous);
    addKnob("lpf_peak",    "PEAK",             MS20KnobStyle::SmallUnipolar);
    addKnob("lpf_mg_ext",  "MG/T.EXT",         MS20KnobStyle::SmallUnipolar);
    addKnob("lpf_eg2_ext", "EG2/EXT",          MS20KnobStyle::SmallUnipolar);

    // Column 6 Bottom: Modulation Generator
    addKnob("mg_wave",     "WAVE FORM", MS20KnobStyle::SmallMGWave);
    addKnob("mg_freq",     "FREQUENCY", MS20KnobStyle::SmallUnipolar);

    // Column 7 Bottom: Envelope Generator 1
    addKnob("eg1_delay",   "DELAY TIME",   MS20KnobStyle::SmallUnipolar);
    addKnob("eg1_attack",  "ATTACK TIME",  MS20KnobStyle::SmallUnipolar);
    addKnob("eg1_release", "RELEASE TIME", MS20KnobStyle::SmallUnipolar);

    // Column 8: Envelope Generator 2 (5 vertical knobs)
    addKnob("eg2_hold",    "HOLD TIME",     MS20KnobStyle::SmallUnipolar);
    addKnob("eg2_attack",  "ATTACK TIME",   MS20KnobStyle::SmallUnipolar);
    addKnob("eg2_decay",   "DECAY TIME",    MS20KnobStyle::SmallUnipolar);
    addKnob("eg2_sustain", "SUSTAIN LEVEL", MS20KnobStyle::SmallUnipolar);
    addKnob("eg2_release", "RELEASE TIME",  MS20KnobStyle::SmallUnipolar);

    // Right Patch Panel Top-Right: VOLUME (Large Knob)
    addKnob("master_volume", "POWER OFF", MS20KnobStyle::LargeContinuous);

    // Right Patch Panel Bottom: ESP (5 Knobs)
    addKnob("esp_sig_level", "SIGNAL LEVEL",    MS20KnobStyle::SmallUnipolar);
    addKnob("esp_low_cut",   "LOW CUT FREQ",    MS20KnobStyle::SmallUnipolar);
    addKnob("esp_high_cut",  "HIGH CUT FREQ",   MS20KnobStyle::SmallUnipolar);
    addKnob("esp_cv_adjust", "CV ADJUST",       MS20KnobStyle::SmallUnipolar);
    addKnob("esp_threshold", "THRESHOLD LEVEL", MS20KnobStyle::SmallUnipolar);

    addAndMakeVisible(wheelDeck);

    // 37-key MS-20 Keyboard (C2 = MIDI 36 to C5 = MIDI 72, 22 white keys)
    midiKeyboard.setAvailableRange(36, 72);
    midiKeyboard.setLowestVisibleKey(36);
    midiKeyboard.setScrollButtonsVisible(false);
    midiKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFFEBEAE2));
    midiKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF181A1D));
    midiKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xFF2C2F33));
    midiKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x44FFD54F));
    midiKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x88E53935));
    addAndMakeVisible(midiKeyboard);

    setSize(1360, 820);
    startTimerHz(30);
}

MS20AudioProcessorEditor::~MS20AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MS20AudioProcessorEditor::addKnob(const juce::String& paramId,
                                       const juce::String& bottomLabel,
                                       MS20KnobStyle style)
{
    KnobEntry entry;
    entry.paramId = paramId;
    entry.bottomLabel = bottomLabel;
    entry.style = style;

    entry.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                  juce::Slider::NoTextBox);
    entry.slider->getProperties().set("ms20Style", static_cast<int>(style));

    if (style == MS20KnobStyle::Selector4)
    {
        // 4-step selector angle span: -54 deg to +54 deg from top (matching real MS-20 rotary switch)
        const float startRad = kTwoPi - (54.0f * kPi / 180.0f);
        const float endRad   = kTwoPi + (54.0f * kPi / 180.0f);
        entry.slider->setRotaryParameters(startRad, endRad, true);
        entry.diameter = 82;
    }
    else
    {
        // Standard 300-degree pot travel (-150 deg to +150 deg from top)
        const float startRad = kTwoPi - (150.0f * kPi / 180.0f);
        const float endRad   = kTwoPi + (150.0f * kPi / 180.0f);
        entry.slider->setRotaryParameters(startRad, endRad, true);
        entry.diameter = (style == MS20KnobStyle::LargeContinuous) ? 90 : 70;
    }

    addAndMakeVisible(*entry.slider);
    entry.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), paramId, *entry.slider);

    knobs.push_back(std::move(entry));
}

MS20AudioProcessorEditor::KnobEntry* MS20AudioProcessorEditor::findKnob(const juce::String& paramId)
{
    for (auto& k : knobs)
        if (k.paramId == paramId)
            return &k;
    return nullptr;
}

void MS20AudioProcessorEditor::resized()
{
    // Compact top utility strip (y: 4..30)
    presetBox.setBounds(34, 5, 290, 24);
    clearPatchBtn.setBounds(332, 5, 120, 24);
    holdDroneBtn.setBounds(460, 5, 95, 24);

    auto place = [this](const juce::String& id, float cx, float cy)
    {
        if (auto* k = findKnob(id))
        {
            k->center = { cx, cy };
            const int d = k->diameter;
            k->slider->setBounds(static_cast<int>(cx) - d / 2, static_cast<int>(cy) - d / 2, d, d);
        }
    };

    // --- 8-Column Left Panel Exact Coordinates (matching ms20_left_zoom.jpg) ---
    // Col 1 (x = 82): VCO 1 & Portamento
    place("vco1_wave",   82.0f, 134.0f);
    place("vco1_pw",     82.0f, 252.0f);
    place("vco1_scale",  82.0f, 368.0f);
    place("portamento",  82.0f, 522.0f);

    // Col 2 (x = 178): VCO 2 & Master Tune
    place("vco2_wave",   178.0f, 134.0f);
    place("vco2_pitch",  178.0f, 252.0f);
    place("vco2_scale",  178.0f, 368.0f);
    place("master_tune", 178.0f, 522.0f);

    // Col 3 (x = 273): VCO Mixer & Frequency Modulation
    place("vco1_level",  273.0f, 150.0f);
    place("vco2_level",  273.0f, 268.0f);
    place("fm_mg_ext",   273.0f, 424.0f);
    place("fm_eg1_ext",  273.0f, 528.0f);

    // Col 4 (x = 367): VC HPF & Cutoff Freq Modulation
    place("hpf_cutoff",  367.0f, 144.0f);
    place("hpf_peak",    367.0f, 268.0f);
    place("hpf_mg_ext",  367.0f, 424.0f);
    place("hpf_eg2_ext", 367.0f, 528.0f);

    // Col 5 (x = 461): VC LPF & Cutoff Freq Modulation
    place("lpf_cutoff",  461.0f, 144.0f);
    place("lpf_peak",    461.0f, 268.0f);
    place("lpf_mg_ext",  461.0f, 424.0f);
    place("lpf_eg2_ext", 461.0f, 528.0f);

    // Col 6 Bottom (x = 553): Modulation Generator
    place("mg_wave",     553.0f, 424.0f);
    place("mg_freq",     553.0f, 528.0f);

    // Col 7 Bottom (x = 644): Envelope Generator 1
    place("eg1_delay",   644.0f, 344.0f);
    place("eg1_attack",  644.0f, 438.0f);
    place("eg1_release", 644.0f, 532.0f);

    // Col 8 (x = 736): Envelope Generator 2 (5 vertical knobs)
    place("eg2_hold",    736.0f, 148.0f);
    place("eg2_attack",  736.0f, 246.0f);
    place("eg2_decay",   736.0f, 344.0f);
    place("eg2_sustain", 736.0f, 438.0f);
    place("eg2_release", 736.0f, 532.0f);

    // --- Right Patch Panel Knobs (matching ms20_patch_zoom.jpg) ---
    // Master Volume knob at top right (between INITIAL GAIN and SIGNAL OUT / PHONES)
    place("master_volume", 1200.0f, 134.0f);

    // 5 ESP Knobs along bottom of ESP section (y = 532)
    place("esp_sig_level", 826.0f,  532.0f);
    place("esp_low_cut",   902.0f,  532.0f);
    place("esp_high_cut",  978.0f,  532.0f);
    place("esp_cv_adjust", 1054.0f, 532.0f);
    place("esp_threshold", 1130.0f, 532.0f);

    layoutPatchJacks();

    // Bottom Left Wheel & Momentary Switch Deck
    wheelDeck.setBounds(34, 608, 112, 196);

    // Bottom Right 37-key Keyboard (22 white keys -> 22 * 52.0f = 1144px wide)
    midiKeyboard.setKeyWidth(52.0f);
    midiKeyboard.setBounds(164, 608, 1144, 196);
}

void MS20AudioProcessorEditor::layoutPatchJacks()
{
    using PS = ms20::dsp::PatchSource;
    using PD = ms20::dsp::PatchDest;

    jacks.clear();
    auto addSrc = [this](PS s, const char* topLbl, const char* botLbl, float x, float y, bool gnd = false)
    {
        jacks.push_back({ true, static_cast<int>(s), topLbl, botLbl, { x, y }, gnd });
    };
    auto addDst = [this](PD d, const char* topLbl, const char* botLbl, float x, float y, bool gnd = false)
    {
        jacks.push_back({ false, static_cast<int>(d), topLbl, botLbl, { x, y }, gnd });
    };

    // Row 1 (y = 148): 6 Modulation/Audio Inputs under the 6 top block diagram boxes, plus SIGNAL OUT & PHONES on far right
    addDst(PD::TotalExt_In,    "TOTAL",          "-5V~+5V",  826.0f,  148.0f);
    addDst(PD::FreqExt_In,     "FREQ",           "-5V~+5V",  892.0f,  148.0f);
    addDst(PD::ExtSig_In,      "EXT SIGNAL IN",  "3VPP MAX", 958.0f,  148.0f);
    addDst(PD::HpfCutoff_In,   "CUTOFF FREQ",    "-5V~+5V",  1024.0f, 148.0f);
    addDst(PD::LpfCutoff_In,   "CUTOFF FREQ",    "-5V~+5V",  1090.0f, 148.0f);
    addDst(PD::InitialGain_In, "INITIAL GAIN",   "0V~+5V",   1142.0f, 148.0f);

    // Far-right vertical column (x = 1286): SIGNAL OUT, PHONES, KBD CV OUT, TRIG OUT, MOMENTARY SW
    addSrc(PS::Signal_Out,     "SIGNAL OUT",     "",         1286.0f, 102.0f);
    addSrc(PS::Phones_Out,     "PHONES",         "",         1286.0f, 162.0f);

    // Row 2 (y = 226): MG Outputs, EG1 Out, EG2 Rev Out, VCO 1+2 CV In, KBD CV Out
    addSrc(PS::MG_TriOut,      "OUT",            "+-2.5V",   826.0f,  226.0f);
    addSrc(PS::MG_RectOut,     "OUT",            "+5V/0V",   918.0f,  226.0f);
    addSrc(PS::EG1_Out,        "OUT",            "",         968.0f,  226.0f);
    addSrc(PS::EG2_RevOut,     "REV OUT",        "0V/-5V",   1136.0f, 226.0f);
    addDst(PD::Vco12Cv_In,     "VCO 1+2 CV IN",  "",         1230.0f, 226.0f);
    addSrc(PS::KbdCv_Out,      "KBD CV OUT",     "",         1286.0f, 226.0f);

    // Row 3 (y = 294): S/H Clock, EG1 Rev Out, MVCA Control Input, EG1 Trig In, VCO 2 CV In, Trig In, Trig Out
    addDst(PD::SH_ClockIn,     "CLOCK",          "",         872.0f,  294.0f, true);
    addSrc(PS::EG1_RevOut,     "REV OUT",        "+5V/0V",   968.0f,  294.0f);
    addDst(PD::MVCA_ControlIn, "CONTROL IN PUT", "0V~+5V",   1020.0f, 294.0f);
    addDst(PD::Eg1Trig_In,     "EG 1 TRIG IN",   "",         1086.0f, 294.0f, true);
    addDst(PD::Vco2Cv_In,      "VCO 2 CV IN",    "",         1180.0f, 266.0f);
    addDst(PD::Eg12Trig_In,    "TRIG IN",        "",         1230.0f, 294.0f, true);
    addSrc(PS::KbdTrig_Out,    "TRIG OUT",       "",         1286.0f, 294.0f, true);

    // Row 4 (y = 364): S/H In & Out, MVCA In & Out, Pink & White Noise, Wheel Out, Momentary Switch Out
    addDst(PD::SH_In,          "IN",             "",         826.0f,  364.0f);
    addSrc(PS::SH_Out,         "OUT",            "",         918.0f,  364.0f);
    addDst(PD::MVCA_In,        "IN",             "",         968.0f,  364.0f);
    addSrc(PS::MVCA_Out,       "OUT",            "",         1042.0f, 364.0f);
    addSrc(PS::PinkNoise_Out,  "PINK",           "",         1094.0f, 364.0f);
    addSrc(PS::WhiteNoise_Out, "WHITE",          "",         1176.0f, 364.0f);
    addSrc(PS::Wheel_Out,      "",               "-5V~+5V",  1230.0f, 364.0f);
    addSrc(PS::MomentarySw_Out,"",               "",         1286.0f, 364.0f, true);

    // Row 5 (y = 452): External Signal Processor (ESP) Jacks matching ms20_patch_zoom.jpg
    addDst(PD::ESP_SigIn,      "SIGNAL IN",      "",         826.0f,  452.0f);
    addSrc(PS::ESP_AmpOut,     "OUT",            "",         920.0f,  452.0f);
    addSrc(PS::ESP_FilterOut,  "OUT",            "",         1014.0f, 452.0f);
    addSrc(PS::ESP_FvCvOut,    "F~V CV OUT",     "",         1108.0f, 452.0f);
    addSrc(PS::ESP_EnvOut,     "ENV OUT",        "",         1230.0f, 452.0f);
    addSrc(PS::ESP_TrigOut,    "TRIG OUT",       "",         1286.0f, 452.0f, true);
}

int MS20AudioProcessorEditor::hitTestJack(juce::Point<float> pt, float maxDist) const
{
    for (int i = 0; i < static_cast<int>(jacks.size()); ++i)
    {
        if (jacks[static_cast<size_t>(i)].center.getDistanceFrom(pt) <= maxDist)
            return i;
    }
    return -1;
}

void MS20AudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const int idx = hitTestJack(pt, 15.0f);
    if (idx < 0)
        return;

    auto& pb = processorRef.getSynthEngine().getPatchBay();
    const auto& jack = jacks[static_cast<size_t>(idx)];

    if (e.mods.isRightButtonDown() || e.mods.isAltDown())
    {
        if (jack.isSource)
            pb.disconnectSource(static_cast<ms20::dsp::PatchSource>(jack.index));
        else
            pb.disconnectDest(static_cast<ms20::dsp::PatchDest>(jack.index));
        repaint();
        return;
    }

    isDraggingCable = true;
    dragStartJackIdx = idx;
    dragCurrentPt = pt;
    repaint();
}

void MS20AudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingCable)
    {
        dragCurrentPt = e.position;
        hoveredJackIdx = hitTestJack(dragCurrentPt, 17.0f);
        repaint();
    }
}

void MS20AudioProcessorEditor::mouseUp(const juce::MouseEvent& e)
{
    if (!isDraggingCable)
        return;

    const int endIdx = hitTestJack(e.position, 17.0f);
    if (endIdx >= 0 && endIdx != dragStartJackIdx)
    {
        const auto& j1 = jacks[static_cast<size_t>(dragStartJackIdx)];
        const auto& j2 = jacks[static_cast<size_t>(endIdx)];
        auto& pb = processorRef.getSynthEngine().getPatchBay();

        if (j1.isSource && !j2.isSource)
        {
            pb.connect(static_cast<ms20::dsp::PatchSource>(j1.index),
                       static_cast<ms20::dsp::PatchDest>(j2.index));
        }
        else if (!j1.isSource && j2.isSource)
        {
            pb.connect(static_cast<ms20::dsp::PatchSource>(j2.index),
                       static_cast<ms20::dsp::PatchDest>(j1.index));
        }
    }

    isDraggingCable = false;
    dragStartJackIdx = -1;
    repaint();
}

void MS20AudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = hitTestJack(e.position, 15.0f);
    if (newHover != hoveredJackIdx)
    {
        hoveredJackIdx = newHover;
        repaint();
    }
}

void MS20AudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int idx = hitTestJack(e.position, 15.0f);
    if (idx >= 0)
    {
        auto& pb = processorRef.getSynthEngine().getPatchBay();
        const auto& jack = jacks[static_cast<size_t>(idx)];
        if (jack.isSource)
            pb.disconnectSource(static_cast<ms20::dsp::PatchSource>(jack.index));
        else
            pb.disconnectDest(static_cast<ms20::dsp::PatchDest>(jack.index));
        repaint();
    }
}

void MS20AudioProcessorEditor::timerCallback()
{
    repaint();
}

void MS20AudioProcessorEditor::drawScrew(juce::Graphics& g, float cx, float cy, float r, bool isRivet) const
{
    g.setColour(juce::Colour(0x99000000));
    g.fillEllipse(cx - r, cy - r + 1.0f, r * 2.0f, r * 2.0f);

    if (isRivet)
    {
        // Smooth matte black panel rivet cap (seen next to VCO/VCF knobs in ms20_left_zoom.jpg)
        juce::ColourGradient rg(juce::Colour(0xFF32353A), cx - r * 0.4f, cy - r * 0.4f,
                                juce::Colour(0xFF141619), cx + r * 0.5f, cy + r * 0.5f, false);
        g.setGradientFill(rg);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(0xFF0A0B0D));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
    }
    else
    {
        // Dark anodized Phillips chassis screw
        juce::ColourGradient sg(juce::Colour(0xFF3E4248), cx - r * 0.4f, cy - r * 0.4f,
                                juce::Colour(0xFF16181C), cx + r * 0.5f, cy + r * 0.5f, false);
        g.setGradientFill(sg);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(0xFF555A60));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.9f);
        g.setColour(juce::Colour(0xFF0D0E10));
        g.drawLine(cx - r * 0.55f, cy - r * 0.25f, cx + r * 0.55f, cy + r * 0.25f, 1.3f);
        g.drawLine(cx - r * 0.25f, cy + r * 0.55f, cx + r * 0.25f, cy - r * 0.55f, 1.3f);
    }
}

void MS20AudioProcessorEditor::drawLed(juce::Graphics& g, juce::Point<float> center, bool on) const
{
    const float r = 4.6f;
    if (on)
    {
        juce::ColourGradient glow(juce::Colour(0xFFFF2A22), center.x, center.y,
                                  juce::Colour(0x00FF0000), center.x + r * 2.8f, center.y + r * 2.8f, true);
        g.setGradientFill(glow);
        g.fillEllipse(center.x - r * 2.8f, center.y - r * 2.8f, r * 5.6f, r * 5.6f);
    }

    // Black plastic LED bezel collar
    g.setColour(juce::Colour(0xFF0E1012));
    g.fillEllipse(center.x - r - 2.2f, center.y - r - 2.2f, (r + 2.2f) * 2.0f, (r + 2.2f) * 2.0f);
    g.setColour(juce::Colour(0xFF383C42));
    g.drawEllipse(center.x - r - 2.2f, center.y - r - 2.2f, (r + 2.2f) * 2.0f, (r + 2.2f) * 2.0f, 0.8f);

    g.setColour(on ? juce::Colour(0xFFFF3828) : juce::Colour(0xFF3E1616));
    g.fillEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f);

    if (on)
    {
        g.setColour(juce::Colour(0xFFFFC8C0));
        g.fillEllipse(center.x - r * 0.45f, center.y - r * 0.45f, r * 0.65f, r * 0.65f);
    }
}

void MS20AudioProcessorEditor::drawSelectorScaleLabels(juce::Graphics& g, const KnobEntry& k) const
{
    const float cx = k.center.x;
    const float cy = k.center.y;
    const float r = static_cast<float>(k.diameter) * 0.295f * 1.56f;

    const float startRad = kTwoPi - (54.0f * kPi / 180.0f);
    const float endRad   = kTwoPi + (54.0f * kPi / 180.0f);

    g.setColour(juce::Colour(0xFFEAEAE2));
    g.setFont(juce::FontOptions(8.8f, juce::Font::bold));

    for (int i = 0; i < 4; ++i)
    {
        const float t = static_cast<float>(i) / 3.0f;
        const float a = startRad + t * (endRad - startRad) - kHalfPi;
        const float lx = cx + std::cos(a) * r;
        const float ly = cy + std::sin(a) * r - 2.0f;

        if (k.paramId == "vco1_wave")
        {
            if (i == 0) drawWaveformIcon(g, "tri",   lx, ly, 9.0f, 6.0f);
            if (i == 1) drawWaveformIcon(g, "saw",   lx, ly, 8.0f, 6.0f);
            if (i == 2) drawWaveformIcon(g, "pw",    lx + 3.0f, ly, 12.0f, 6.0f);
            if (i == 3) drawWaveformIcon(g, "noise", lx + 5.0f, ly, 13.0f, 6.0f);
        }
        else if (k.paramId == "vco2_wave")
        {
            if (i == 0) drawWaveformIcon(g, "saw",    lx, ly, 8.0f, 6.0f);
            if (i == 1) drawWaveformIcon(g, "sq",     lx, ly, 9.0f, 6.0f);
            if (i == 2) drawWaveformIcon(g, "narrow", lx, ly, 9.0f, 6.0f);
            if (i == 3) g.drawText("RING", static_cast<int>(lx - 6.0f), static_cast<int>(ly - 5.0f), 28, 11, juce::Justification::centredLeft, false);
        }
        else if (k.paramId == "vco1_scale")
        {
            static constexpr const char* s1[4] = { "32'", "16'", "8'", "4'" };
            g.drawText(s1[i], static_cast<int>(lx - 11.0f), static_cast<int>(ly - 6.0f), 22, 12, juce::Justification::centred, false);
        }
        else if (k.paramId == "vco2_scale")
        {
            static constexpr const char* s2[4] = { "16'", "8'", "4'", "2'" };
            g.drawText(s2[i], static_cast<int>(lx - 11.0f), static_cast<int>(ly - 6.0f), 22, 12, juce::Justification::centred, false);
        }
    }
}

void MS20AudioProcessorEditor::drawLeftControlPanel(juce::Graphics& g) const
{
    g.setColour(juce::Colour(0xFFE8E8E0));

    // Exact dividing lines from ms20_left_zoom.jpg
    // Vertical column lines
    g.drawLine(130.0f, 38.0f, 130.0f, 594.0f, 1.3f); // Between VCO 1 and VCO 2
    g.drawLine(226.0f, 38.0f, 226.0f, 62.0f,  1.3f); // Top tick between VCO 2 and VCO MIXER
    g.drawLine(226.0f, 326.0f, 226.0f, 594.0f, 1.3f);// Lower line between VCO 2 and FREQ MOD
    g.drawLine(320.0f, 38.0f, 320.0f, 594.0f, 1.3f); // Between VCO MIXER and HPF
    g.drawLine(414.0f, 38.0f, 414.0f, 594.0f, 1.3f); // Between HPF and LPF
    g.drawLine(508.0f, 38.0f, 508.0f, 594.0f, 1.3f); // Right of LPF
    g.drawLine(599.0f, 240.0f, 599.0f, 594.0f, 1.3f);// Between MG and EG 1
    g.drawLine(690.0f, 38.0f, 690.0f, 594.0f, 1.3f); // Between VCA/EG1 and EG 2

    // Stepped thick white boundary separating VCO Mixer/HPF/LPF/VCA top from FM/Cutoff Mod/MG/EG1 bottom
    g.drawLine(34.0f,  438.0f, 226.0f, 438.0f, 1.3f); // Above PORTAMENTO and MASTER TUNE
    g.drawLine(226.0f, 336.0f, 508.0f, 336.0f, 2.0f); // Above FREQ MOD and CUTOFF FREQ MOD
    g.drawLine(508.0f, 240.0f, 690.0f, 240.0f, 2.0f); // Below VOLTAGE CONTROLLED AMPLIFIER
    g.drawLine(508.0f, 240.0f, 508.0f, 336.0f, 2.0f); // Vertical step connecting y=240 to y=336

    // Thick white divider between Left Control Section (Col 8 EG 2) and Right Patch Panel
    g.drawLine(782.0f, 38.0f, 782.0f, 594.0f, 2.2f);

    // Top Column Titles (Directly silk-screened in crisp uppercase white sans-serif like the photo)
    g.setFont(juce::FontOptions(8.2f, juce::Font::bold));
    auto drawColTitle = [&g](float x, float y, float w, const char* line1, const char* line2 = nullptr)
    {
        g.drawText(line1, static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), 12, juce::Justification::centred, false);
        if (line2 != nullptr)
            g.drawText(line2, static_cast<int>(x), static_cast<int>(y + 10.5f), static_cast<int>(w), 12, juce::Justification::centred, false);
    };

    drawColTitle(34.0f,  45.0f, 96.0f,  "VOLTAGE CONTROLLED", "OSCILLATOR 1");
    drawColTitle(130.0f, 45.0f, 96.0f,  "VOLTAGE CONTROLLED", "OSCILLATOR 2");
    drawColTitle(226.0f, 45.0f, 94.0f,  "VCO MIXER");
    drawColTitle(320.0f, 45.0f, 94.0f,  "VOLTAGE CONTROLLED", "HIGHPASS FILTER");
    drawColTitle(414.0f, 45.0f, 94.0f,  "VOLTAGE CONTROLLED", "LOWPASS FILTER");
    drawColTitle(508.0f, 45.0f, 182.0f, "VOLTAGE CONTROLLED AMPLIFIER");
    drawColTitle(690.0f, 45.0f, 92.0f,  "ENVELOPE", "GENERATOR 2");

    // Lower Sub-section Titles
    drawColTitle(34.0f,  446.0f, 96.0f, "PORTAMENTO");
    drawColTitle(130.0f, 446.0f, 96.0f, "MASTER TUNE");
    drawColTitle(226.0f, 344.0f, 94.0f, "FREQUENCY", "MODULATION");
    drawColTitle(320.0f, 344.0f, 94.0f, "CUTOFF FREQUENCY", "MODULATION");
    drawColTitle(414.0f, 344.0f, 94.0f, "CUTOFF FREQUENCY", "MODULATION");
    drawColTitle(508.0f, 250.0f, 91.0f, "MODULATION", "GENERATOR");
    drawColTitle(599.0f, 250.0f, 91.0f, "ENVELOPE", "GENERATOR 1");

    // Silk-screened VCA Amplifier Triangle Symbol in VOLTAGE CONTROLLED AMPLIFIER box (Col 6+7 top)
    const float vcaCx = 599.0f;
    const float vcaCy = 148.0f;
    juce::Path vcaTri;
    vcaTri.startNewSubPath(vcaCx - 20.0f, vcaCy - 20.0f);
    vcaTri.lineTo(vcaCx - 20.0f, vcaCy + 20.0f);
    vcaTri.lineTo(vcaCx + 18.0f, vcaCy);
    vcaTri.closeSubPath();
    g.strokePath(vcaTri, juce::PathStrokeType(1.3f));
    g.drawLine(vcaCx - 46.0f, vcaCy, vcaCx - 20.0f, vcaCy, 1.3f); // Input line
    g.drawLine(vcaCx + 18.0f, vcaCy, vcaCx + 46.0f, vcaCy, 1.3f); // Output line
    drawArrowLine(g, vcaCx - 2.0f, vcaCy + 42.0f, vcaCx - 2.0f, vcaCy + 11.0f, 4.5f); // Control arrow from bottom
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("EG 2 & EXT", static_cast<int>(vcaCx - 35.0f), static_cast<int>(vcaCy + 45.0f), 66, 11, juce::Justification::centred, false);

    // Draw selector switch labels & bottom labels for all left-panel knobs
    for (const auto& k : knobs)
    {
        if (k.style == MS20KnobStyle::Selector4)
            drawSelectorScaleLabels(g, k);

        if (k.bottomLabel.isNotEmpty() && k.center.x < 780.0f)
        {
            g.setFont(juce::FontOptions(8.4f, juce::Font::bold));
            const int ly = static_cast<int>(k.center.y + static_cast<float>(k.diameter) * 0.40f);
            g.drawText(k.bottomLabel, static_cast<int>(k.center.x - 46.0f), ly, 92, 12,
                       juce::Justification::centred, false);
        }
    }

    // Black panel rivet caps & screws in exact positions from ms20_left_zoom.jpg
    drawScrew(g, 44.0f,  98.0f,  6.0f, false); // Top-left Phillips screw
    drawScrew(g, 44.0f,  550.0f, 6.0f, false); // Bottom-left Phillips screw
    drawScrew(g, 118.0f, 300.0f, 5.2f, true);  // Rivet below VCO1 PW
    drawScrew(g, 118.0f, 424.0f, 5.2f, true);  // Rivet below VCO1 Scale
    drawScrew(g, 118.0f, 560.0f, 5.2f, true);  // Rivet below Portamento
    drawScrew(g, 214.0f, 300.0f, 5.2f, true);  // Rivet below VCO2 Pitch
    drawScrew(g, 214.0f, 424.0f, 5.2f, true);  // Rivet below VCO2 Scale
    drawScrew(g, 332.0f, 208.0f, 5.2f, true);  // Rivet below HPF Cutoff
    drawScrew(g, 426.0f, 208.0f, 5.2f, true);  // Rivet below LPF Cutoff
}

void MS20AudioProcessorEditor::drawRightPatchPanel(juce::Graphics& g) const
{
    g.setColour(juce::Colour(0xFFE8E8E0));

    // 1. Top Row Block Diagram Boxes: VCO 1 -> VCO 2 -> VCO MIXER -> HP FILTER -> LP FILTER -> AMPLIFIER -> VOLUME -> SIGNAL OUT
    auto drawBox = [&g](float cx, float cy, float w, float h, const char* l1, const char* l2 = nullptr, const char* l3 = nullptr)
    {
        const juce::Rectangle<float> r(cx - w * 0.5f, cy - h * 0.5f, w, h);
        g.drawRect(r, 1.2f);
        g.setFont(juce::FontOptions(7.3f, juce::Font::bold));
        if (l3 != nullptr)
        {
            g.drawText(l1, static_cast<int>(r.getX()), static_cast<int>(r.getY() + 1.5f), static_cast<int>(w), 8, juce::Justification::centred, false);
            g.drawText(l2, static_cast<int>(r.getX()), static_cast<int>(r.getY() + 9.0f), static_cast<int>(w), 8, juce::Justification::centred, false);
            g.drawText(l3, static_cast<int>(r.getX()), static_cast<int>(r.getY() + 16.5f), static_cast<int>(w), 8, juce::Justification::centred, false);
        }
        else if (l2 != nullptr)
        {
            g.drawText(l1, static_cast<int>(r.getX()), static_cast<int>(r.getY() + 5.0f), static_cast<int>(w), 9, juce::Justification::centred, false);
            g.drawText(l2, static_cast<int>(r.getX()), static_cast<int>(r.getY() + 14.0f), static_cast<int>(w), 9, juce::Justification::centred, false);
        }
        else
        {
            g.drawText(l1, static_cast<int>(r.getX()), static_cast<int>(r.getY()), static_cast<int>(w), static_cast<int>(h), juce::Justification::centred, false);
        }
    };

    const float topY = 74.0f;
    drawBox(826.0f,  topY, 56.0f, 28.0f, "VOLTAGE", "CONTROLLED", "OSCILLATOR 1");
    drawBox(892.0f,  topY, 56.0f, 28.0f, "VOLTAGE", "CONTROLLED", "OSCILLATOR 2");
    drawBox(958.0f,  topY, 44.0f, 28.0f, "VCO", "MIXER");
    drawBox(1024.0f, topY, 56.0f, 28.0f, "VOLTAGE", "CONTROLLED", "HP FILTER");
    drawBox(1090.0f, topY, 56.0f, 28.0f, "VOLTAGE", "CONTROLLED", "LP FILTER");
    drawBox(1148.0f, topY, 48.0f, 28.0f, "VOLTAGE", "CONTROLLED", "AMPLIFIER");

    // Signal flow arrows along the top row
    drawArrowLine(g, 854.0f,  topY, 864.0f,  topY, 3.5f);
    drawArrowLine(g, 920.0f,  topY, 936.0f,  topY, 3.5f);
    drawArrowLine(g, 980.0f,  topY, 996.0f,  topY, 3.5f);
    drawArrowLine(g, 1052.0f, topY, 1062.0f, topY, 3.5f);
    drawArrowLine(g, 1118.0f, topY, 1124.0f, topY, 3.5f);
    drawArrowLine(g, 1172.0f, topY, 1270.0f, topY, 3.5f);

    // Upward modulation arrows from Row 1 jacks (y=148) into top boxes (orthogonal routing like hardware)
    drawArrowLine(g, 826.0f,  122.0f, 826.0f,  88.0f, 3.5f);
    drawArrowLine(g, 892.0f,  122.0f, 892.0f,  88.0f, 3.5f);
    g.drawLine(958.0f, 122.0f, 958.0f, 104.0f, 1.15f);
    g.drawLine(958.0f, 104.0f, 988.0f, 104.0f, 1.15f);
    drawArrowLine(g, 988.0f,  104.0f, 988.0f,  74.0f, 3.5f);
    drawArrowLine(g, 1024.0f, 122.0f, 1024.0f, 88.0f, 3.5f);
    drawArrowLine(g, 1090.0f, 122.0f, 1090.0f, 88.0f, 3.5f);
    drawArrowLine(g, 1142.0f, 122.0f, 1142.0f, 88.0f, 3.5f);

    // VOLUME header above Master Volume knob
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("VOLUME", 1168, 52, 64, 12, juce::Justification::centred, false);
    g.drawText("POWER OFF", 1166, 178, 68, 11, juce::Justification::centred, false);

    // 2. Middle Block Diagram Boxes (matching ms20_patch_zoom.jpg)
    drawBox(872.0f,  226.0f, 52.0f, 26.0f, "MODULATION", "GENERATOR");
    drawArrowLine(g, 846.0f, 226.0f, 840.0f, 226.0f, 3.5f);
    drawArrowLine(g, 898.0f, 226.0f, 904.0f, 226.0f, 3.5f);

    drawBox(1020.0f, 226.0f, 50.0f, 26.0f, "ENVELOPE", "GENERATOR", "1");
    drawArrowLine(g, 995.0f, 226.0f, 984.0f, 226.0f, 3.5f);

    drawBox(1086.0f, 226.0f, 50.0f, 26.0f, "ENVELOPE", "GENERATOR", "2");
    drawArrowLine(g, 1111.0f, 226.0f, 1120.0f, 226.0f, 3.5f);

    // Normalled orthogonal routing lines from MG Out -> TOTAL, EG1 Out -> FREQ, EG2 -> HPF/LPF Cutoff
    drawArrowLine(g, 826.0f, 200.0f, 826.0f, 174.0f, 3.5f);
    g.drawLine(968.0f, 200.0f, 968.0f, 188.0f, 1.15f);
    g.drawLine(892.0f, 188.0f, 968.0f, 188.0f, 1.15f);
    drawArrowLine(g, 892.0f, 188.0f, 892.0f, 174.0f, 3.5f);

    g.drawLine(1086.0f, 213.0f, 1086.0f, 188.0f, 1.15f);
    g.drawLine(1024.0f, 188.0f, 1090.0f, 188.0f, 1.15f);
    drawArrowLine(g, 1024.0f, 188.0f, 1024.0f, 174.0f, 3.5f);
    drawArrowLine(g, 1090.0f, 188.0f, 1090.0f, 174.0f, 3.5f);

    // Sample & Hold Box (Row 4 left)
    drawBox(872.0f,  364.0f, 50.0f, 24.0f, "SAMPLE", "& HOLD");
    drawArrowLine(g, 872.0f, 314.0f, 872.0f, 352.0f, 3.5f); // Clock -> S/H
    drawArrowLine(g, 840.0f, 364.0f, 847.0f, 364.0f, 3.5f); // In -> S/H
    drawArrowLine(g, 897.0f, 364.0f, 904.0f, 364.0f, 3.5f); // S/H -> Out

    // Patch-Bay VCA Triangle Symbol (Row 4 center)
    const float mvcaX = 1005.0f;
    const float mvcaY = 364.0f;
    juce::Path mvcaTri;
    mvcaTri.startNewSubPath(mvcaX - 12.0f, mvcaY - 12.0f);
    mvcaTri.lineTo(mvcaX - 12.0f, mvcaY + 12.0f);
    mvcaTri.lineTo(mvcaX + 12.0f, mvcaY);
    mvcaTri.closeSubPath();
    g.strokePath(mvcaTri, juce::PathStrokeType(1.2f));
    g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
    g.drawText("VCA", static_cast<int>(mvcaX - 13.0f), static_cast<int>(mvcaY - 5.0f), 22, 10, juce::Justification::centred, false);
    g.drawLine(982.0f, mvcaY, mvcaX - 12.0f, mvcaY, 1.15f);
    drawArrowLine(g, mvcaX + 12.0f, mvcaY, 1028.0f, mvcaY, 3.5f);
    g.drawLine(1020.0f, 314.0f, 1020.0f, 336.0f, 1.15f);
    g.drawLine(1002.0f, 336.0f, 1020.0f, 336.0f, 1.15f);
    drawArrowLine(g, 1002.0f, 336.0f, 1002.0f, mvcaY - 7.0f, 3.5f); // Control In -> VCA

    // Noise Generator Box (Row 4 right)
    drawBox(1135.0f, 364.0f, 46.0f, 24.0f, "NOISE", "GENERATOR");
    drawArrowLine(g, 1112.0f, 364.0f, 1108.0f, 364.0f, 3.5f);
    drawArrowLine(g, 1158.0f, 364.0f, 1162.0f, 364.0f, 3.5f);

    // KBD Box between VCO 1+2 CV IN / KBD CV OUT and TRIG IN / TRIG OUT
    drawBox(1258.0f, 260.0f, 26.0f, 14.0f, "KBD");

    // Wheel & Momentary Switch silk-screened icons above their jacks at (1230, 364) and (1286, 364)
    juce::Path wheelIcon;
    wheelIcon.addCentredArc(1230.0f, 342.0f, 9.0f, 6.0f, 0.0f, -1.2f, 1.2f, true);
    g.strokePath(wheelIcon, juce::PathStrokeType(1.2f));
    g.drawLine(1280.0f, 338.0f, 1292.0f, 338.0f, 1.5f);
    g.drawLine(1286.0f, 338.0f, 1286.0f, 344.0f, 1.5f);

    // 3. Horizontal White Divider above EXTERNAL SIGNAL PROCESSOR (y = 402)
    g.drawLine(790.0f, 402.0f, 1316.0f, 402.0f, 1.8f);
    g.setFont(juce::FontOptions(9.8f, juce::Font::bold));
    g.drawText("EXTERNAL SIGNAL PROCESSOR", 845, 407, 230, 13, juce::Justification::centred, false);

    // ESP Block Diagram: SIGNAL IN -> |> AMP -> OUT -> [BAND PASS FILTER] -> OUT -> [F-V CONVERTER] -> F~V CV OUT -> [ENVELOPE FOLLOWER] -> ENV OUT -> TRIG OUT
    const float espY = 452.0f;
    juce::Path ampTri;
    ampTri.startNewSubPath(862.0f, espY - 12.0f);
    ampTri.lineTo(862.0f, espY + 12.0f);
    ampTri.lineTo(886.0f, espY);
    ampTri.closeSubPath();
    g.strokePath(ampTri, juce::PathStrokeType(1.2f));
    g.setFont(juce::FontOptions(7.2f, juce::Font::bold));
    g.drawText("AMP", 861, static_cast<int>(espY - 5.0f), 20, 10, juce::Justification::centred, false);

    drawBox(967.0f,  espY, 50.0f, 24.0f, "BAND PASS", "FILTER");
    drawBox(1061.0f, espY, 50.0f, 24.0f, "F-V", "CONVERTER");
    drawBox(1176.0f, espY, 50.0f, 24.0f, "ENVELOPE", "FOLLOWER");

    // ESP horizontal signal flow lines & lower branch from AMP out to ENVELOPE FOLLOWER
    g.drawLine(840.0f, espY, 862.0f, espY, 1.15f);
    drawArrowLine(g, 886.0f, espY, 906.0f, espY, 3.5f);
    g.drawLine(934.0f, espY, 942.0f, espY, 1.15f);
    drawArrowLine(g, 992.0f, espY, 1000.0f, espY, 3.5f);
    g.drawLine(1028.0f, espY, 1036.0f, espY, 1.15f);
    drawArrowLine(g, 1086.0f, espY, 1094.0f, espY, 3.5f);
    g.drawLine(896.0f, espY, 896.0f, espY + 26.0f, 1.15f);
    g.drawLine(896.0f, espY + 26.0f, 1138.0f, espY + 26.0f, 1.15f);
    g.drawLine(1138.0f, espY + 26.0f, 1138.0f, espY, 1.15f);
    drawArrowLine(g, 1138.0f, espY, 1151.0f, espY, 3.5f);
    drawArrowLine(g, 1201.0f, espY, 1216.0f, espY, 3.5f);
    drawArrowLine(g, 1244.0f, espY, 1272.0f, espY, 3.5f);

    // ESP Knob Labels along bottom
    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    for (const auto& k : knobs)
    {
        if (k.center.x >= 780.0f && k.paramId != "master_volume")
        {
            const int ly = static_cast<int>(k.center.y + static_cast<float>(k.diameter) * 0.40f);
            g.drawText(k.bottomLabel, static_cast<int>(k.center.x - 42.0f), ly, 84, 12,
                       juce::Justification::centred, false);
        }
    }

    // Iconic KORG SYNTHESIZER logo at bottom-right of the ESP section (matching ms20_patch_zoom.jpg!)
    g.setColour(juce::Colour(0xFFEAEAE2));
    g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    g.drawText("KORG", 1185, 505, 118, 32, juce::Justification::centredLeft, false);
    g.setFont(juce::FontOptions(14.5f, juce::Font::plain));
    g.drawText("SYNTHESIZER", 1185, 538, 125, 18, juce::Justification::centredLeft, false);

    // Right edge chassis Phillips screws
    drawScrew(g, 1314.0f, 118.0f, 6.0f, false);
    drawScrew(g, 1314.0f, 536.0f, 6.0f, false);
}

void MS20AudioProcessorEditor::drawPatchJack(juce::Graphics& g,
                                             const JackVisual& jack,
                                             bool isHovered,
                                             bool isConnected) const
{
    const auto c = jack.center;
    const float rOuter = 11.2f;

    // Top & bottom silk-screened labels around jack
    g.setColour(juce::Colour(0xFFE8E8E0));
    g.setFont(juce::FontOptions(7.6f, juce::Font::bold));
    if (jack.topLabel.isNotEmpty())
    {
        g.drawText(jack.topLabel, static_cast<int>(c.x - 32.0f), static_cast<int>(c.y - 23.0f),
                   64, 10, juce::Justification::centred, false);
    }
    if (jack.bottomLabel.isNotEmpty())
    {
        g.drawText(jack.bottomLabel, static_cast<int>(c.x - 30.0f), static_cast<int>(c.y + 13.0f),
                   60, 10, juce::Justification::centred, false);
    }
    if (jack.hasGndSymbol)
    {
        g.setFont(juce::FontOptions(6.8f, juce::Font::bold));
        g.drawText("GND", static_cast<int>(c.x + 8.0f), static_cast<int>(c.y + 10.0f),
                   24, 9, juce::Justification::centredLeft, false);
    }

    // Drop shadow beneath chrome nut
    g.setColour(juce::Colour(0x99000000));
    g.fillEllipse(c.x - rOuter, c.y - rOuter + 1.5f, rOuter * 2.0f, rOuter * 2.0f);

    // Chrome washer & hex nut matching ms20_patch_zoom.jpg
    g.setColour(isHovered ? juce::Colour(0xFFFFD54F) : juce::Colour(0xFFD0D4D8));
    g.fillEllipse(c.x - rOuter, c.y - rOuter, rOuter * 2.0f, rOuter * 2.0f);
    g.setColour(juce::Colour(0xFF282B30));
    g.drawEllipse(c.x - rOuter, c.y - rOuter, rOuter * 2.0f, rOuter * 2.0f, 1.0f);

    // Chamfered inner chrome ring
    g.setColour(juce::Colour(0xFFEFF2F5));
    g.fillEllipse(c.x - 8.2f, c.y - 8.2f, 16.4f, 16.4f);
    g.setColour(juce::Colour(0xFF4A4E54));
    g.drawEllipse(c.x - 8.2f, c.y - 8.2f, 16.4f, 16.4f, 0.9f);

    // Deep dark 1/4" bore
    g.setColour(isConnected ? juce::Colour(0xFF24282E) : juce::Colour(0xFF08090B));
    g.fillEllipse(c.x - 5.4f, c.y - 5.4f, 10.8f, 10.8f);
}

void MS20AudioProcessorEditor::drawCatenaryCable(juce::Graphics& g,
                                                 juce::Point<float> p1,
                                                 juce::Point<float> p2,
                                                 juce::Colour col,
                                                 float alpha) const
{
    const float dist = p1.getDistanceFrom(p2);
    const float sag = std::min(90.0f, 16.0f + dist * 0.22f);
    const juce::Point<float> ctrl((p1.x + p2.x) * 0.5f, std::max(p1.y, p2.y) + sag);

    juce::Path cablePath;
    cablePath.startNewSubPath(p1);
    cablePath.quadraticTo(ctrl, p2);

    juce::Path shadowPath = cablePath;
    shadowPath.applyTransform(juce::AffineTransform::translation(2.0f, 4.0f));
    g.setColour(juce::Colour(0x66000000));
    g.strokePath(shadowPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(col.withAlpha(alpha));
    g.strokePath(cablePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(col.brighter(0.45f).withAlpha(alpha * 0.65f));
    g.strokePath(cablePath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xFF1A1C20).withAlpha(alpha));
    g.fillEllipse(p1.x - 5.2f, p1.y - 5.2f, 10.4f, 10.4f);
    g.fillEllipse(p2.x - 5.2f, p2.y - 5.2f, 10.4f, 10.4f);
    g.setColour(col.withAlpha(alpha));
    g.fillEllipse(p1.x - 3.0f, p1.y - 3.0f, 6.0f, 6.0f);
    g.fillEllipse(p2.x - 3.0f, p2.y - 3.0f, 6.0f, 6.0f);
}

void MS20AudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Studio Black Background & Molded Side Cheeks
    g.fillAll(juce::Colour(0xFF0D0E10));

    // Left & right structural side cheeks (seen on the edges of the Wikimedia photo)
    juce::ColourGradient cheekGrad(juce::Colour(0xFF282B30), 0.0f, 34.0f,
                                   juce::Colour(0xFF121417), 0.0f, 815.0f, false);
    g.setGradientFill(cheekGrad);
    g.fillRoundedRectangle(6.0f, 32.0f, 26.0f, 780.0f, 3.0f);
    g.fillRoundedRectangle(1328.0f, 32.0f, 26.0f, 780.0f, 3.0f);
    g.setColour(juce::Colour(0xFF3E4248));
    g.drawRoundedRectangle(6.0f, 32.0f, 26.0f, 780.0f, 3.0f, 1.0f);
    g.drawRoundedRectangle(1328.0f, 32.0f, 26.0f, 780.0f, 3.0f, 1.0f);

    // Main Matte Dark-Charcoal Powder-Coated Metal Panel (x: 34..1326, y: 38..594)
    juce::ColourGradient panelGrad(juce::Colour(0xFF25282C), 680.0f, 38.0f,
                                   juce::Colour(0xFF191B1E), 680.0f, 594.0f, false);
    g.setGradientFill(panelGrad);
    g.fillRect(34.0f, 34.0f, 1292.0f, 564.0f);

    // Crisp White Silk-Screened Outer Perimeter Border (matching the photo)
    g.setColour(juce::Colour(0xFFE8E8E0));
    g.drawRect(34.0f, 38.0f, 1292.0f, 556.0f, 1.8f);

    // Subtle hint in the top utility strip
    g.setColour(juce::Colour(0xFF9EA4AA));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("KORG MS-20 (KLM-127 / 128C / 129 Circuit Model)  |  Drag between jacks to patch  |  Right-click jack to unplug",
               570, 5, 755, 24, juce::Justification::centredRight, false);

    // 2. Render Left Control Section & Right Patch Panel Silk-Screen Artwork
    drawLeftControlPanel(g);
    drawRightPatchPanel(g);

    // 3. Draw All 35 Chrome Patch Jacks
    const auto& pb = processorRef.getSynthEngine().getPatchBay();
    for (int i = 0; i < static_cast<int>(jacks.size()); ++i)
    {
        const auto& jack = jacks[static_cast<size_t>(i)];
        bool connected = false;
        if (!jack.isSource)
        {
            connected = pb.isConnected(static_cast<ms20::dsp::PatchDest>(jack.index));
        }
        else
        {
            for (int d = 0; d < ms20::dsp::MS20PatchBay::numDests; ++d)
            {
                if (pb.getConnectedSourceIndex(static_cast<ms20::dsp::PatchDest>(d)) == jack.index)
                {
                    connected = true;
                    break;
                }
            }
        }
        drawPatchJack(g, jack, i == hoveredJackIdx, connected);
    }

    // 4. Draw the 4 Hardware Red Indicator LEDs at their exact positions in the photo:
    //    - VCA Trigger LED (under "VOLTAGE CONTROLLED AMPLIFIER" header at x=599, y=74)
    //    - MG Rate LED (above MG WAVE FORM knob at x=553, y=338)
    //    - ESP PEAK LED (above ENVELOPE FOLLOWER box at x=1176, y=422)
    //    - ESP TRIG LED (above ESP TRIG OUT jack at x=1286, y=422)
    const auto& eng = processorRef.getSynthEngine();
    drawLed(g, { 599.0f,  74.0f  }, eng.getKbdTrigLed());
    drawLed(g, { 553.0f,  338.0f }, eng.getMgRateLed());
    drawLed(g, { 1176.0f, 420.0f }, eng.getEspPeakLed());
    g.setColour(juce::Colour(0xFFE8E8E0));
    g.setFont(juce::FontOptions(7.4f, juce::Font::bold));
    g.drawText("PEAK", 1156, 427, 40, 10, juce::Justification::centred, false);
    drawLed(g, { 1286.0f, 420.0f }, eng.getEspTrigLed());

    // 5. Keyboard shadow & red felt strip above the 37-key keyboard
    g.setColour(juce::Colour(0xFF070809));
    g.fillRect(156, 598, 1162, 10);
    g.setColour(juce::Colour(0xFF7B1F1F));
    g.fillRect(164, 605, 1144, 3);
}

void MS20AudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const auto cables = processorRef.getSynthEngine().getPatchBay().getActiveCables();

    auto findJackCenter = [this](bool isSource, int enumIdx) -> juce::Point<float>
    {
        for (const auto& j : jacks)
            if (j.isSource == isSource && j.index == enumIdx)
                return j.center;
        return {};
    };

    for (const auto& c : cables)
    {
        const auto p1 = findJackCenter(true,  static_cast<int>(c.source));
        const auto p2 = findJackCenter(false, static_cast<int>(c.dest));
        if (!p1.isOrigin() && !p2.isOrigin())
            drawCatenaryCable(g, p1, p2, juce::Colour(c.colorArgb), 0.90f);
    }

    if (isDraggingCable && dragStartJackIdx >= 0 && dragStartJackIdx < static_cast<int>(jacks.size()))
    {
        const auto p1 = jacks[static_cast<size_t>(dragStartJackIdx)].center;
        drawCatenaryCable(g, p1, dragCurrentPt, juce::Colour(0xFFFFD54F), 0.95f);
    }
}
