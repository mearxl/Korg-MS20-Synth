#include "PluginProcessor.h"
#include "PluginEditor.h"

MS20AudioProcessor::MS20AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("ESP External Input", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "MS20Parameters", createParameterLayout())
{
    initFactoryPresets();
    loadFactoryPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout MS20AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- VCO 1 ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "vco1_wave", 1 }, "VCO 1 Waveform",
        juce::StringArray { "Triangle", "Sawtooth", "Pulse (PW)", "White Noise" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vco1_pw", 1 }, "VCO 1 Pulse Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "vco1_scale", 1 }, "VCO 1 Scale",
        juce::StringArray { "32'", "16'", "8'", "4'" }, 2));

    // --- VCO 2 ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "vco2_wave", 1 }, "VCO 2 Waveform",
        juce::StringArray { "Sawtooth", "Square", "Narrow Pulse", "Ring Mod" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vco2_pitch", 1 }, "VCO 2 Pitch",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "vco2_scale", 1 }, "VCO 2 Scale",
        juce::StringArray { "16'", "8'", "4'", "2'" }, 1));

    // --- VCO Master & FM ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "portamento", 1 }, "Portamento",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_tune", 1 }, "Master Tune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fm_mg_ext", 1 }, "Freq Mod MG/T.EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fm_eg1_ext", 1 }, "Freq Mod EG1/EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // --- VCO Mixer ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vco1_level", 1 }, "VCO 1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.80f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vco2_level", 1 }, "VCO 2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // --- VC HPF (KORG-35 IC9) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "hpf_cutoff", 1 }, "HPF Cutoff Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "hpf_peak", 1 }, "HPF Peak",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "hpf_mg_ext", 1 }, "HPF Mod MG/T.EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "hpf_eg2_ext", 1 }, "HPF Mod EG2/EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // --- VC LPF (KORG-35 IC10) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lpf_cutoff", 1 }, "LPF Cutoff Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lpf_peak", 1 }, "LPF Peak",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lpf_mg_ext", 1 }, "LPF Mod MG/T.EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lpf_eg2_ext", 1 }, "LPF Mod EG2/EXT",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // --- MG (Modulation Generator) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mg_wave", 1 }, "MG Waveform",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mg_freq", 1 }, "MG Frequency",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.45f));

    // --- EG 1 ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg1_delay", 1 }, "EG 1 Delay Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg1_attack", 1 }, "EG 1 Attack Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.02f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg1_release", 1 }, "EG 1 Release Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    // --- EG 2 ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg2_hold", 1 }, "EG 2 Hold Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg2_attack", 1 }, "EG 2 Attack Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg2_decay", 1 }, "EG 2 Decay Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.38f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg2_sustain", 1 }, "EG 2 Sustain Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "eg2_release", 1 }, "EG 2 Release Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.28f));

    // --- ESP (KLM-129) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "esp_sig_level", 1 }, "ESP Signal Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "esp_low_cut", 1 }, "ESP Low Cut Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "esp_high_cut", 1 }, "ESP High Cut Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "esp_cv_adjust", 1 }, "ESP CV Adjust",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.625f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "esp_threshold", 1 }, "ESP Threshold",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    // --- Manual Controller & Master ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "control_wheel", 1 }, "Control Wheel",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "momentary_sw", 1 }, "Momentary Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master_volume", 1 }, "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.80f));

    return { params.begin(), params.end() };
}

void MS20AudioProcessor::initFactoryPresets()
{
    using PS = ms20::dsp::PatchSource;
    using PD = ms20::dsp::PatchDest;

    // Helper lambda to construct a complete 37-parameter MS-20 preset from 0..10 hardware dial values
    auto makePatch = [](juce::String name,
                        int vco1Wave, float vco1Pw, int vco1Scale,
                        int vco2Wave, float vco2PitchSemis, int vco2Scale,
                        float portamento, float masterTuneCents,
                        float vco1Level, float vco2Level,
                        float fmMgExt, float fmEg1Ext,
                        float hpfCutoff, float hpfPeak, float hpfMgExt, float hpfEg2Ext,
                        float lpfCutoff, float lpfPeak, float lpfMgExt, float lpfEg2Ext,
                        float mgWave, float mgFreq,
                        float eg1Delay, float eg1Attack, float eg1Release,
                        float eg2Hold, float eg2Attack, float eg2Decay, float eg2Sustain, float eg2Release,
                        float espSigLevel = 0.0f, float espLowCut = 0.0f, float espHighCut = 10.0f,
                        float espCvAdjust = 5.0f, float espThreshold = 0.0f,
                        std::vector< std::pair<PS, PD> > cables = {}) -> FactoryPreset
    {
        return {
            std::move(name),
            {
                { "vco1_wave", static_cast<float>(vco1Wave) },
                { "vco1_pw", vco1Pw * 0.1f },
                { "vco1_scale", static_cast<float>(vco1Scale) },
                { "vco2_wave", static_cast<float>(vco2Wave) },
                { "vco2_pitch", vco2PitchSemis },
                { "vco2_scale", static_cast<float>(vco2Scale) },
                { "portamento", portamento * 0.1f },
                { "master_tune", masterTuneCents },
                { "vco1_level", vco1Level * 0.1f },
                { "vco2_level", vco2Level * 0.1f },
                { "fm_mg_ext", fmMgExt * 0.1f },
                { "fm_eg1_ext", fmEg1Ext * 0.1f },
                { "hpf_cutoff", hpfCutoff * 0.1f },
                { "hpf_peak", hpfPeak * 0.1f },
                { "hpf_mg_ext", hpfMgExt * 0.1f },
                { "hpf_eg2_ext", hpfEg2Ext * 0.1f },
                { "lpf_cutoff", lpfCutoff * 0.1f },
                { "lpf_peak", lpfPeak * 0.1f },
                { "lpf_mg_ext", lpfMgExt * 0.1f },
                { "lpf_eg2_ext", lpfEg2Ext * 0.1f },
                { "mg_wave", mgWave * 0.1f },
                { "mg_freq", mgFreq * 0.1f },
                { "eg1_delay", eg1Delay * 0.1f },
                { "eg1_attack", eg1Attack * 0.1f },
                { "eg1_release", eg1Release * 0.1f },
                { "eg2_hold", eg2Hold * 0.1f },
                { "eg2_attack", eg2Attack * 0.1f },
                { "eg2_decay", eg2Decay * 0.1f },
                { "eg2_sustain", eg2Sustain * 0.1f },
                { "eg2_release", eg2Release * 0.1f },
                { "esp_sig_level", espSigLevel * 0.1f },
                { "esp_low_cut", espLowCut * 0.1f },
                { "esp_high_cut", espHighCut * 0.1f },
                { "esp_cv_adjust", espCvAdjust * 0.1f },
                { "esp_threshold", espThreshold * 0.1f },
                { "control_wheel", 0.0f },
                { "master_volume", 0.80f }
            },
            std::move(cables)
        };
    };

    factoryPresets = {
        // 1. Default Init Patch (Most Basic Single-VCO Saw)
        makePatch("1. Init Patch (Basic)",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 1,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  8.0f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  10.0f, 0.0f, 0.0f, 0.0f,
                  /*MG*/   5.0f, 4.5f,
                  /*EG1*/  0.0f, 0.0f, 1.5f,
                  /*EG2*/  0.0f, 0.0f, 3.5f, 10.0f, 1.2f),

        // =========================================================================
        // KORG MS-20 OFFICIAL PATCH BOOK — BASS (Pages 4–10)
        // =========================================================================
        // Page 4: Punchy Saw Bass
        makePatch("2. Bass: Punchy Saw Bass",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 0, 0.0f, 1,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.5f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  3.5f, 0.8f, 0.0f, 10.0f,
                  /*MG*/   5.0f, 3.5f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 3.9f, 4.7f, 0.0f),

        // Page 5: 8ve Reso Bass
        makePatch("3. Bass: 8ve Reso Bass",
                  /*VCO1*/ 0, 3.2f, 1,  /*VCO2*/ 1, 0.05f, 0, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.0f, 7.5f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  2.5f, 4.1f, 0.0f, 8.5f,
                  /*MG*/   5.0f, 0.3f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 1.6f, 0.5f, 0.3f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_RevOut, PD::LpfCutoff_In } }),

        // Page 6: Acid Saw Bass
        makePatch("4. Bass: Acid Saw Bass",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 1.2f, 0.0f,
                  /*Mix*/  9.5f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  3.5f, 8.0f, 6.7f, 9.1f,
                  /*MG*/   5.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 3.9f,
                  /*EG2*/  0.0f, 0.0f, 5.5f, 0.0f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_RevOut, PD::InitialGain_In } }),

        // Page 7: Hoover Switch Bass
        makePatch("5. Bass: Hoover Switch Bass",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, -0.35f, 0, /*Port/Tune*/ 2.2f, 0.0f,
                  /*Mix*/  9.5f, 9.5f,  /*FM*/   0.0f, 5.3f,
                  /*HPF*/  0.3f, 5.3f, 0.0f, 6.1f,
                  /*LPF*/  7.3f, 5.0f, 0.0f, 0.0f,
                  /*MG*/   5.0f, 3.9f,
                  /*EG1*/  0.0f, 0.8f, 0.3f,
                  /*EG2*/  0.0f, 0.0f, 5.4f, 9.7f, 0.1f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_Out, PD::FreqExt_In }, { PS::MomentarySw_Out, PD::Eg1Trig_In } }),

        // Page 8: Flat Bass
        makePatch("6. Bass: Flat Bass",
                  /*VCO1*/ 2, 0.0f, 1,  /*VCO2*/ 2, 0.0f, 1,  /*Port/Tune*/ 2.2f, 0.0f,
                  /*Mix*/  9.5f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  0.0f, 2.1f, 0.0f, 10.0f,
                  /*MG*/   5.0f, 0.0f,
                  /*EG1*/  0.0f, 0.8f, 3.9f,
                  /*EG2*/  0.0f, 0.0f, 9.9f, 9.6f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_Out, PD::LpfCutoff_In } }),

        // Page 9: PW Arp Bass
        makePatch("7. Bass: PW Arp Bass",
                  /*VCO1*/ 2, 6.7f, 1,  /*VCO2*/ 0, 0.08f, 1, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.5f, 9.5f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  5.4f, 2.1f, 9.8f, 0.0f,
                  /*MG*/   0.0f, 2.1f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 9.9f, 9.6f, 0.0f),

        // Page 10: Arp Dub Bass
        makePatch("8. Bass: Arp Dub Bass",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 0, 0.12f, 0, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.5f, 9.5f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  5.3f, 5.5f, 9.8f, 0.0f,
                  /*MG*/   10.0f, 2.1f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 9.9f, 9.7f, 0.0f),

        // =========================================================================
        // KORG MS-20 OFFICIAL PATCH BOOK — DRUM (Pages 11–16)
        // =========================================================================
        // Page 11: Kick
        makePatch("9. Drum: Kick",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  0.0f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  1.5f, 9.6f, 3.1f, 5.7f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.6f, 0.0f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::KbdTrig_Out, PD::TotalExt_In } }),

        // Page 12: Kick Sustain
        makePatch("10. Drum: Kick Sustain",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  0.0f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  1.8f, 9.6f, 3.0f, 5.7f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 3.9f,
                  /*EG2*/  0.0f, 0.0f, 0.6f, 0.0f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::KbdTrig_Out, PD::TotalExt_In }, { PS::EG1_Out, PD::InitialGain_In } }),

        // Page 13: MS Clap
        makePatch("11. Drum: MS Clap",
                  /*VCO1*/ 3, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  8.3f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  6.3f, 6.3f, 6.3f, 0.0f,
                  /*LPF*/  7.0f, 7.1f, 7.0f, 0.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.4f, 0.0f, 0.2f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::Phones_Out, PD::TotalExt_In } }),

        // Page 14: MS Hi-Hat
        makePatch("12. Drum: MS Hi-Hat",
                  /*VCO1*/ 3, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  8.8f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  8.2f, 2.3f, 0.0f, 2.1f,
                  /*LPF*/  9.7f, 0.0f, 0.0f, 0.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.1f, 0.0f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::KbdCv_Out, PD::HpfCutoff_In } }),

        // Page 15: MS Hi-Hat Open
        makePatch("13. Drum: MS Hi-Hat Open",
                  /*VCO1*/ 3, 0.0f, 2,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  8.8f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  8.1f, 2.4f, 0.0f, 2.1f,
                  /*LPF*/  9.6f, 0.0f, 0.0f, 0.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 1.0f, 0.0f, 0.7f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::KbdCv_Out, PD::HpfCutoff_In } }),

        // Page 16: MS Snare
        makePatch("14. Drum: MS Snare",
                  /*VCO1*/ 0, 0.0f, 2,  /*VCO2*/ 2, 0.0f, 3,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 2.1f,  /*FM*/   0.0f, 4.8f,
                  /*HPF*/  4.4f, 2.1f, 0.0f, 0.0f,
                  /*LPF*/  9.1f, 1.3f, 0.0f, 0.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.5f,
                  /*EG2*/  0.0f, 0.0f, 0.4f, 0.0f, 0.0f,
                  10.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::PinkNoise_Out, PD::ExtSig_In } }),

        // =========================================================================
        // KORG MS-20 OFFICIAL PATCH BOOK — KEYBOARD (Pages 17–21)
        // =========================================================================
        // Page 17: Arp Hit 1
        makePatch("15. Keyboard: Arp Hit 1",
                  /*VCO1*/ 1, 0.0f, 3,  /*VCO2*/ 3, 0.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 9.4f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  3.3f, 7.0f, 0.0f, 10.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 1.2f, 2.0f, 0.0f,
                  6.5f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::Signal_Out, PD::ExtSig_In } }),

        // Page 18: Arp Goes Forth
        makePatch("16. Keyboard: Arp Goes Forth",
                  /*VCO1*/ 1, 0.0f, 3,  /*VCO2*/ 0, 5.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.1f, 9.4f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  6.3f, 5.6f, 1.9f, 7.2f,
                  /*LPF*/  6.3f, 0.9f, 9.7f, 8.1f,
                  /*MG*/   0.0f, 1.9f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 7.1f, 4.0f, 3.3f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::Wheel_Out, PD::LpfCutoff_In } }),

        // Page 19: MS Bell
        makePatch("17. Keyboard: MS Bell",
                  /*VCO1*/ 0, 0.0f, 2,  /*VCO2*/ 3, 7.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  7.4f, 5.7f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.9f, 1.4f, 0.0f, 5.7f,
                  /*LPF*/  7.6f, 4.5f, 0.0f, 2.4f,
                  /*MG*/   2.8f, 2.5f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.5f, 0.0f, 0.4f),

        // Page 20: Synth Brass
        makePatch("18. Keyboard: Synth Brass",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 0.15f, 1, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 9.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  5.3f, 4.5f, 0.0f, 10.0f,
                  /*MG*/   9.9f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.4f, 4.0f, 5.5f, 0.8f),

        // Page 21: Ring Mod Key Track
        makePatch("19. Keyboard: Ring Mod Key Track",
                  /*VCO1*/ 2, 5.5f, 1,  /*VCO2*/ 3, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 9.5f,  /*FM*/   0.0f, 0.9f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 3.9f,
                  /*LPF*/  4.9f, 7.1f, 0.0f, 10.0f,
                  /*MG*/   0.0f, 3.9f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 10.0f, 9.7f, 3.9f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::Wheel_Out, PD::FreqExt_In }, { PS::KbdCv_Out, PD::LpfCutoff_In } }),

        // =========================================================================
        // KORG MS-20 OFFICIAL PATCH BOOK — LEAD (Pages 22–28)
        // =========================================================================
        // Page 22: Soft Portamento Lead
        makePatch("20. Lead: Soft Portamento Lead",
                  /*VCO1*/ 0, 0.0f, 1,  /*VCO2*/ 1, 0.08f, 1, /*Port/Tune*/ 1.5f, 0.0f,
                  /*Mix*/  9.1f, 9.4f,  /*FM*/   0.0f, 1.5f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 4.1f,
                  /*LPF*/  5.7f, 3.3f, 0.0f, 2.1f,
                  /*MG*/   5.5f, 3.3f,
                  /*EG1*/  5.1f, 5.1f, 0.8f,
                  /*EG2*/  0.0f, 0.5f, 0.0f, 9.0f, 1.5f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::MG_TriOut, PD::MVCA_In }, { PS::EG1_Out, PD::MVCA_ControlIn }, { PS::MVCA_Out, PD::FreqExt_In } }),

        // Page 23: 8ve Lead
        makePatch("21. Lead: 8ve Lead",
                  /*VCO1*/ 2, 7.5f, 1,  /*VCO2*/ 2, 12.0f, 1, /*Port/Tune*/ 0.9f, 0.0f,
                  /*Mix*/  8.4f, 7.8f,  /*FM*/   0.0f, 1.5f,
                  /*HPF*/  4.4f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  9.3f, 0.0f, 0.0f, 0.0f,
                  /*MG*/   4.9f, 3.3f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.0f, 9.7f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::MG_TriOut, PD::MVCA_In }, { PS::Wheel_Out, PD::MVCA_ControlIn }, { PS::MVCA_Out, PD::FreqExt_In } }),

        // Page 24: Unison Saw Lead
        makePatch("22. Lead: Unison Saw Lead",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 0, 0.15f, 0, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 9.4f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  7.3f, 0.1f, 0.0f, 0.0f,
                  /*MG*/   5.4f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.1f, 4.9f, 9.7f, 0.0f),

        // Page 25: Reso Lead
        makePatch("23. Lead: Reso Lead",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 2, 0.08f, 2, /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  7.5f, 3.5f,  /*FM*/   0.0f, 0.7f,
                  /*HPF*/  0.0f, 1.1f, 0.0f, 0.0f,
                  /*LPF*/  4.9f, 7.5f, 0.0f, 9.7f,
                  /*MG*/   5.5f, 0.0f,
                  /*EG1*/  0.0f, 0.6f, 0.2f,
                  /*EG2*/  0.0f, 0.4f, 6.5f, 5.7f, 0.2f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_Out, PD::FreqExt_In }, { PS::MomentarySw_Out, PD::Eg1Trig_In } }),

        // Page 26: Daft Lead
        makePatch("24. Lead: Daft Lead",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 1, 0.08f, 1, /*Port/Tune*/ 2.1f, 0.0f,
                  /*Mix*/  9.2f, 9.4f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  4.5f, 7.7f, 0.0f, 8.2f,
                  /*LPF*/  6.8f, 7.9f, 0.0f, 0.0f,
                  /*MG*/   5.4f, 0.0f,
                  /*EG1*/  2.5f, 1.6f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 2.8f, 0.0f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::EG1_RevOut, PD::HpfCutoff_In } }),

        // Page 27: 5th Saw Lead
        makePatch("25. Lead: 5th Saw Lead",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 7.0f, 1,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  7.5f, 7.9f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  3.3f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  6.6f, 3.5f, 0.0f, 0.7f,
                  /*MG*/   5.3f, 3.9f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 3.7f, 6.8f, 0.4f),

        // Page 28: Reed Lead
        makePatch("26. Lead: Reed Lead",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 2, 7.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 7.5f,  /*FM*/   0.0f, 0.7f,
                  /*HPF*/  3.9f, 6.6f, 2.3f, 8.2f,
                  /*LPF*/  5.7f, 6.9f, 4.7f, 0.7f,
                  /*MG*/   5.4f, 0.3f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 6.4f, 3.1f, 0.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::Wheel_Out, PD::FreqExt_In } }),

        // =========================================================================
        // KORG MS-20 OFFICIAL PATCH BOOK — SOUND EFFECT (Pages 29–33)
        // =========================================================================
        // Page 29: 5th Sample & Hold
        makePatch("27. SE: 5th Sample & Hold",
                  /*VCO1*/ 1, 0.0f, 2,  /*VCO2*/ 0, 7.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  6.9f, 7.1f,  /*FM*/   0.0f, 7.5f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 8.2f,
                  /*LPF*/  4.4f, 7.5f, 0.0f, 7.2f,
                  /*MG*/   9.7f, 2.3f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 2.1f, 0.0f, 9.7f, 2.1f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::PinkNoise_Out, PD::SH_In }, { PS::MG_RectOut, PD::SH_ClockIn }, { PS::SH_Out, PD::LpfCutoff_In } }),

        // Page 30: Space Attack Hold or Release
        makePatch("28. SE: Space Attack Hold or Release",
                  /*VCO1*/ 1, 0.0f, 1,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  0.0f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  0.0f, 9.6f, 4.2f, 10.0f,
                  /*MG*/   5.5f, 9.1f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 2.8f, 0.0f, 9.0f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::MG_RectOut, PD::TotalExt_In } }),

        // Page 31: Explosion
        makePatch("29. SE: Explosion",
                  /*VCO1*/ 2, 5.3f, 3,  /*VCO2*/ 3, 0.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  2.8f, 6.6f, 0.0f, 10.0f,
                  /*LPF*/  5.2f, 4.7f, 5.3f, 8.1f,
                  /*MG*/   0.0f, 3.9f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 1.4f, 7.3f, 8.2f,
                  10.0f, 2.0f, 8.0f, 8.0f, 0.0f,
                  { { PS::PinkNoise_Out, PD::ESP_SigIn }, { PS::ESP_EnvOut, PD::ExtSig_In }, { PS::KbdCv_Out, PD::TotalExt_In } }),

        // Page 32: Mad Prof
        makePatch("30. SE: Mad Prof",
                  /*VCO1*/ 2, 6.7f, 1,  /*VCO2*/ 2, 7.0f, 2,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  9.2f, 9.4f,  /*FM*/   4.3f, 1.1f,
                  /*HPF*/  4.4f, 4.5f, 7.4f, 0.0f,
                  /*LPF*/  6.9f, 7.9f, 7.5f, 7.2f,
                  /*MG*/   9.8f, 4.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 9.9f, 9.6f, 0.7f,
                  0.0f, 0.0f, 10.0f, 5.0f, 0.0f,
                  { { PS::MG_TriOut, PD::SH_In }, { PS::MG_RectOut, PD::SH_ClockIn }, { PS::Wheel_Out, PD::LpfCutoff_In } }),

        // Page 33: MS Zap
        makePatch("31. SE: MS Zap",
                  /*VCO1*/ 0, 0.0f, 1,  /*VCO2*/ 0, 0.0f, 0,  /*Port/Tune*/ 0.0f, 0.0f,
                  /*Mix*/  0.0f, 0.0f,  /*FM*/   0.0f, 0.0f,
                  /*HPF*/  0.0f, 0.0f, 0.0f, 0.0f,
                  /*LPF*/  0.0f, 8.4f, 0.0f, 10.0f,
                  /*MG*/   0.0f, 0.0f,
                  /*EG1*/  0.0f, 0.0f, 0.0f,
                  /*EG2*/  0.0f, 0.0f, 0.5f, 0.0f, 0.0f)
    };
}

void MS20AudioProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(factoryPresets.size()))
        return;

    currentPresetIndex = index;
    const auto& preset = factoryPresets[static_cast<size_t>(index)];

    // Reset all parameters to sane defaults first
    for (auto* param : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    }

    for (const auto& [paramId, val] : preset.params)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    }

    auto& pb = synthEngine.getPatchBay();
    pb.clearAll();
    for (const auto& [src, dst] : preset.cables)
        pb.connect(src, dst);
}

int MS20AudioProcessor::getNumPrograms()
{
    return static_cast<int>(factoryPresets.size());
}

void MS20AudioProcessor::setCurrentProgram(int index)
{
    loadFactoryPreset(index);
}

const juce::String MS20AudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
        return factoryPresets[static_cast<size_t>(index)].name;
    return {};
}

void MS20AudioProcessor::changeProgramName(int, const juce::String&) {}

void MS20AudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    synthEngine.prepare(sampleRate);
    keyboardState.reset();
}

void MS20AudioProcessor::releaseResources()
{
    synthEngine.reset();
}

bool MS20AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    const auto& mainIn = layouts.getMainInputChannelSet();
    if (!mainIn.isDisabled() && mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

ms20::dsp::MS20Params MS20AudioProcessor::readCurrentParams() const noexcept
{
    ms20::dsp::MS20Params p;

    p.vco1Wave     = static_cast<int>(apvts.getRawParameterValue("vco1_wave")->load());
    p.vco1Pw       = static_cast<double>(apvts.getRawParameterValue("vco1_pw")->load());
    p.vco1Scale    = static_cast<int>(apvts.getRawParameterValue("vco1_scale")->load());

    p.vco2Wave     = static_cast<int>(apvts.getRawParameterValue("vco2_wave")->load());
    p.vco2Pitch    = static_cast<double>(apvts.getRawParameterValue("vco2_pitch")->load());
    p.vco2Scale    = static_cast<int>(apvts.getRawParameterValue("vco2_scale")->load());

    p.portamento   = static_cast<double>(apvts.getRawParameterValue("portamento")->load());
    p.masterTune   = static_cast<double>(apvts.getRawParameterValue("master_tune")->load());
    p.fmMgExt      = static_cast<double>(apvts.getRawParameterValue("fm_mg_ext")->load());
    p.fmEg1Ext     = static_cast<double>(apvts.getRawParameterValue("fm_eg1_ext")->load());

    p.vco1Level    = static_cast<double>(apvts.getRawParameterValue("vco1_level")->load());
    p.vco2Level    = static_cast<double>(apvts.getRawParameterValue("vco2_level")->load());

    p.hpfCutoff    = static_cast<double>(apvts.getRawParameterValue("hpf_cutoff")->load());
    p.hpfPeak      = static_cast<double>(apvts.getRawParameterValue("hpf_peak")->load());
    p.hpfMgExt     = static_cast<double>(apvts.getRawParameterValue("hpf_mg_ext")->load());
    p.hpfEg2Ext    = static_cast<double>(apvts.getRawParameterValue("hpf_eg2_ext")->load());

    p.lpfCutoff    = static_cast<double>(apvts.getRawParameterValue("lpf_cutoff")->load());
    p.lpfPeak      = static_cast<double>(apvts.getRawParameterValue("lpf_peak")->load());
    p.lpfMgExt     = static_cast<double>(apvts.getRawParameterValue("lpf_mg_ext")->load());
    p.lpfEg2Ext    = static_cast<double>(apvts.getRawParameterValue("lpf_eg2_ext")->load());

    p.mgWave       = static_cast<double>(apvts.getRawParameterValue("mg_wave")->load());
    p.mgFreq       = static_cast<double>(apvts.getRawParameterValue("mg_freq")->load());

    p.eg1Delay     = static_cast<double>(apvts.getRawParameterValue("eg1_delay")->load());
    p.eg1Attack    = static_cast<double>(apvts.getRawParameterValue("eg1_attack")->load());
    p.eg1Release   = static_cast<double>(apvts.getRawParameterValue("eg1_release")->load());

    p.eg2Hold      = static_cast<double>(apvts.getRawParameterValue("eg2_hold")->load());
    p.eg2Attack    = static_cast<double>(apvts.getRawParameterValue("eg2_attack")->load());
    p.eg2Decay     = static_cast<double>(apvts.getRawParameterValue("eg2_decay")->load());
    p.eg2Sustain   = static_cast<double>(apvts.getRawParameterValue("eg2_sustain")->load());
    p.eg2Release   = static_cast<double>(apvts.getRawParameterValue("eg2_release")->load());

    p.espSigLevel  = static_cast<double>(apvts.getRawParameterValue("esp_sig_level")->load());
    p.espLowCut    = static_cast<double>(apvts.getRawParameterValue("esp_low_cut")->load());
    p.espHighCut   = static_cast<double>(apvts.getRawParameterValue("esp_high_cut")->load());
    p.espCvAdjust  = static_cast<double>(apvts.getRawParameterValue("esp_cv_adjust")->load());
    p.espThreshold = static_cast<double>(apvts.getRawParameterValue("esp_threshold")->load());

    p.controlWheel = static_cast<double>(apvts.getRawParameterValue("control_wheel")->load());
    p.momentarySw  = apvts.getRawParameterValue("momentary_sw")->load() > 0.5f;
    p.masterVolume = static_cast<double>(apvts.getRawParameterValue("master_volume")->load());

    return p;
}

void MS20AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Merge on-screen keyboard MIDI events into midiMessages
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    const auto params = readCurrentParams();
    const float* extInL = (totalNumInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const float* extInR = (totalNumInputChannels > 1) ? buffer.getReadPointer(1) : nullptr;

    // Copy external input before we write output channels in-place
    std::vector<float> extMono(static_cast<size_t>(numSamples), 0.0f);
    if (extInL != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float r = (extInR != nullptr) ? extInR[i] : extInL[i];
            extMono[static_cast<size_t>(i)] = 0.5f * (extInL[i] + r);
        }
    }

    float* outL = (totalNumOutputChannels > 0) ? buffer.getWritePointer(0) : nullptr;
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    auto midiIterator = midiMessages.cbegin();
    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        while (midiIterator != midiMessages.cend())
        {
            const auto metadata = *midiIterator;
            if (metadata.samplePosition > sampleIdx)
                break;

            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
                synthEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                synthEngine.noteOff(msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                synthEngine.allNotesOff();
            else if (msg.isPitchWheel())
            {
                const double normBend = (static_cast<double>(msg.getPitchWheelValue()) - 8192.0) / 8192.0;
                synthEngine.setPitchBendSemitones(normBend * 2.0);
            }
            else if (msg.isController() && msg.getControllerNumber() == 1) // Mod wheel -> Control Wheel
            {
                if (auto* wheelParam = apvts.getParameter("control_wheel"))
                    wheelParam->setValueNotifyingHost(static_cast<float>(msg.getControllerValue()) / 127.0f);
            }

            ++midiIterator;
        }

        const float monoSample = synthEngine.processSample(params, static_cast<double>(extMono[static_cast<size_t>(sampleIdx)]));
        if (outL != nullptr)
            outL[sampleIdx] = monoSample;
        if (outR != nullptr)
            outR[sampleIdx] = monoSample;
    }

    for (int ch = 2; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);
}

void MS20AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree patchTree("PatchBayCables");
    const auto& pb = synthEngine.getPatchBay();
    for (int d = 0; d < ms20::dsp::MS20PatchBay::numDests; ++d)
    {
        const int s = pb.getConnectedSourceIndex(static_cast<ms20::dsp::PatchDest>(d));
        if (s >= 0)
        {
            juce::ValueTree cable("Cable");
            cable.setProperty("src", s, nullptr);
            cable.setProperty("dst", d, nullptr);
            patchTree.addChild(cable, -1, nullptr);
        }
    }
    state.removeChild(state.getChildWithName("PatchBayCables"), nullptr);
    state.addChild(patchTree, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MS20AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(tree);

        auto& pb = synthEngine.getPatchBay();
        pb.clearAll();
        auto patchTree = tree.getChildWithName("PatchBayCables");
        if (patchTree.isValid())
        {
            for (int i = 0; i < patchTree.getNumChildren(); ++i)
            {
                auto cable = patchTree.getChild(i);
                const int s = cable.getProperty("src", -1);
                const int d = cable.getProperty("dst", -1);
                if (s >= 0 && d >= 0)
                    pb.connect(static_cast<ms20::dsp::PatchSource>(s),
                               static_cast<ms20::dsp::PatchDest>(d));
            }
        }
    }
}

juce::AudioProcessorEditor* MS20AudioProcessor::createEditor()
{
    return new MS20AudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MS20AudioProcessor();
}
