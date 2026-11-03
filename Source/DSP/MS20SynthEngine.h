#pragma once

#include "Korg35Filter.h"
#include "MS20Oscillators.h"
#include "MS20Modulators.h"
#include "MS20ESP.h"
#include "MS20PatchBay.h"
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace ms20::dsp
{

struct MS20Params
{
    // VCO 1
    int vco1Wave = 1;          // 0=Tri, 1=Saw, 2=PW, 3=Noise
    double vco1Pw = 0.0;       // 0.0 (50%) .. 1.0 (narrow)
    int vco1Scale = 2;         // 0=32', 1=16', 2=8', 3=4'

    // VCO 2
    int vco2Wave = 0;          // 0=Saw, 1=Sq, 2=Pulse, 3=Ring
    double vco2Pitch = 0.0;    // -12 .. +12 semitones
    int vco2Scale = 1;         // 0=16', 1=8', 2=4', 3=2'

    // VCO Master & FM
    double portamento = 0.0;   // 0..1
    double masterTune = 0.0;   // -100..+100 cents
    double fmMgExt = 0.0;      // 0..1
    double fmEg1Ext = 0.0;     // 0..1

    // VCO Mixer
    double vco1Level = 0.8;    // 0..1
    double vco2Level = 0.6;    // 0..1

    // VC HPF
    double hpfCutoff = 0.0;    // 0..1
    double hpfPeak = 0.0;      // 0..1
    double hpfMgExt = 0.0;     // 0..1
    double hpfEg2Ext = 0.0;    // 0..1

    // VC LPF
    double lpfCutoff = 0.65;   // 0..1
    double lpfPeak = 0.35;     // 0..1
    double lpfMgExt = 0.0;     // 0..1
    double lpfEg2Ext = 0.45;   // 0..1

    // MG
    double mgWave = 0.5;       // 0..1
    double mgFreq = 0.35;      // 0..1

    // EG 1
    double eg1Delay = 0.0;     // 0..1
    double eg1Attack = 0.02;   // 0..1
    double eg1Release = 0.25;  // 0..1

    // EG 2
    double eg2Hold = 0.0;      // 0..1
    double eg2Attack = 0.01;   // 0..1
    double eg2Decay = 0.35;    // 0..1
    double eg2Sustain = 0.65;  // 0..1
    double eg2Release = 0.25;  // 0..1

    // ESP
    double espSigLevel = 0.5;  // 0..1
    double espLowCut = 0.15;   // 0..1
    double espHighCut = 0.75;  // 0..1
    double espCvAdjust = 0.625;// 0..1
    double espThreshold = 0.35;// 0..1

    // Manual Controller & Volume
    double controlWheel = 0.0; // -1..+1
    bool momentarySw = false;
    double masterVolume = 0.8; // 0..1
};

/**
 * Complete circuit-modeled Korg MS-20 Monophonic Synthesizer Engine (KLM-127 / 128 / 129).
 * Runs the VCO + Mixer + Dual KORG-35 VCFs + VCA at 4x internal oversampling.
 */
class MS20SynthEngine
{
public:
    static constexpr int oversampleFactor = 4;

    MS20SynthEngine()
        : hpf(Korg35Filter::Mode::HighPass),
          lpf(Korg35Filter::Mode::LowPass)
    {
        // Authentic subtle component tolerance differences between IC9 (HPF) and IC10 (LPF)
        hpf.setComponentTolerance(1.000, 0.996, 1.02);
        lpf.setComponentTolerance(0.998, 1.003, -0.98);
        heldNotes.fill(false);
    }

    void prepare(double newSampleRate) noexcept
    {
        hostSampleRate = std::max(44100.0, newSampleRate);
        oversampledRate = hostSampleRate * static_cast<double>(oversampleFactor);

        vcos.prepare(oversampledRate);
        noiseGen.prepare(oversampledRate);
        hpf.prepare(oversampledRate);
        lpf.prepare(oversampledRate);

        eg1.prepare(hostSampleRate);
        eg2.prepare(hostSampleRate);
        mg.prepare(hostSampleRate);
        sampleAndHold.prepare(hostSampleRate);
        mvca.prepare(hostSampleRate);
        esp.prepare(hostSampleRate);

        // Parameter smoothing filter at host sample rate (~2.5ms)
        paramSmoothCoeff = 1.0 - std::exp(-1.0 / (0.0025 * hostSampleRate));
        dcBlockCoeff = 1.0 - (2.0 * 3.14159265358979323846 * 12.0 / hostSampleRate);

        reset();
    }

    void reset() noexcept
    {
        vcos.reset();
        noiseGen.reset();
        hpf.reset();
        lpf.reset();
        eg1.reset();
        eg2.reset();
        mg.reset();
        sampleAndHold.reset();
        mvca.reset();
        esp.reset();

        patchSources.fill(0.0);
        lastSignalOut = 0.0;
        lastPhonesOut = 0.0;
        dcBlockX1 = 0.0;
        dcBlockY1 = 0.0;
        aaState1 = 0.0;
        aaState2 = 0.0;
    }

    // --- MIDI Note Handling (Monophonic Low-Note Priority like KLM-127 resistor ladder) ---
    void noteOn(int midiNote, float velocity) noexcept
    {
        if (midiNote < 0 || midiNote >= 128)
            return;

        const bool wasAnyHeld = isAnyNoteHeld();
        const int prevNote = currentMidiNote;

        heldNotes[static_cast<size_t>(midiNote)] = true;
        lastVelocity = std::clamp(static_cast<double>(velocity), 0.1, 1.0);

        updateActiveNoteFromLadder();

        // KLM-127 IC16/Q13 trigger detector generates a retrigger pulse when a new key
        // is pressed from silence, OR when a lower key steals priority on the resistor ladder!
        const bool retrigger = !wasAnyHeld || (currentMidiNote < prevNote);
        kbdGate = true;
        if (retrigger)
            pendingKbdRetrig = true;
    }

    void noteOff(int midiNote) noexcept
    {
        if (midiNote < 0 || midiNote >= 128)
            return;

        heldNotes[static_cast<size_t>(midiNote)] = false;
        if (isAnyNoteHeld())
        {
            updateActiveNoteFromLadder();
            kbdGate = true;
        }
        else
        {
            kbdGate = false;
        }
    }

    void allNotesOff() noexcept
    {
        heldNotes.fill(false);
        kbdGate = false;
    }

    void setPitchBendSemitones(double semitones) noexcept
    {
        pitchBendSemitones = std::clamp(semitones, -12.0, 12.0);
    }

    /**
     * Process a single audio frame (at hostSampleRate) with optional external audio input for ESP.
     */
    inline float processSample(const MS20Params& p, double externalAudioIn = 0.0) noexcept
    {
        // 1. Update Envelope, MG, and ESP parameters
        eg1.setParameters(p.eg1Delay, p.eg1Attack, p.eg1Release);
        eg2.setParameters(p.eg2Hold, p.eg2Attack, p.eg2Decay, p.eg2Sustain, p.eg2Release);
        esp.setParameters(p.espSigLevel, p.espLowCut, p.espHighCut, p.espCvAdjust, p.espThreshold);

        // 2. Compute Keyboard CV (octaves relative to C2 = MIDI 36) with RC Portamento (IC3 TL081 + C7 0.22uF)
        targetKbdCvOct = (static_cast<double>(currentMidiNote - 36) + pitchBendSemitones) / 12.0;
        const double portaTimeSec = (p.portamento <= 0.002)
            ? 0.0003
            : (0.001 + 2.8 * p.portamento * p.portamento);
        const double portaCoeff = 1.0 - std::exp(-1.0 / std::max(1.0, portaTimeSec * hostSampleRate));
        glidedKbdCvOct += portaCoeff * (targetKbdCvOct - glidedKbdCvOct);

        // Populate keyboard & manual controller patch sources
        patchSources[static_cast<size_t>(PatchSource::KbdCv_Out)]       = glidedKbdCvOct;
        patchSources[static_cast<size_t>(PatchSource::KbdTrig_Out)]     = kbdGate ? 1.0 : 0.0;
        patchSources[static_cast<size_t>(PatchSource::Wheel_Out)]       = p.controlWheel;
        patchSources[static_cast<size_t>(PatchSource::MomentarySw_Out)] = p.momentarySw ? 1.0 : 0.0;
        patchSources[static_cast<size_t>(PatchSource::Signal_Out)]      = lastSignalOut;
        patchSources[static_cast<size_t>(PatchSource::Phones_Out)]      = lastPhonesOut;

        // 3. Process ESP (KLM-129)
        // If J1 (ESP_SigIn) is patched on the patch bay, use the patched signal; otherwise use DAW audio input
        const double espInput = patchBay.isConnected(PatchDest::ESP_SigIn)
            ? patchBay.read(PatchDest::ESP_SigIn, patchSources, 0.0)
            : externalAudioIn;
        const auto espOut = esp.processSample(espInput);
        patchSources[static_cast<size_t>(PatchSource::ESP_AmpOut)]    = espOut.ampOut;
        patchSources[static_cast<size_t>(PatchSource::ESP_FilterOut)] = espOut.filterOut;
        patchSources[static_cast<size_t>(PatchSource::ESP_FvCvOut)]   = espOut.fvCvOut;
        patchSources[static_cast<size_t>(PatchSource::ESP_EnvOut)]    = espOut.envOut;
        patchSources[static_cast<size_t>(PatchSource::ESP_TrigOut)]   = espOut.trigOut;

        // 4. Process MG (Modulation Generator)
        const auto mgOut = mg.processSample(p.mgWave, p.mgFreq);
        patchSources[static_cast<size_t>(PatchSource::MG_TriOut)]  = mgOut.triOut;
        patchSources[static_cast<size_t>(PatchSource::MG_RectOut)] = mgOut.rectOut;

        // 5. Determine Trigger inputs for EG-1 and EG-2 (respecting J20 EG1 Trig In & J27 Ext Trig In)
        const bool extTrig12 = patchBay.isConnected(PatchDest::Eg12Trig_In)
            && (patchBay.read(PatchDest::Eg12Trig_In, patchSources, 0.0) > 0.35);

        const bool eg2Gate = kbdGate || extTrig12;
        const bool eg1Gate = patchBay.isConnected(PatchDest::Eg1Trig_In)
            ? (patchBay.read(PatchDest::Eg1Trig_In, patchSources, 0.0) > 0.35)
            : (kbdGate || extTrig12);

        const bool retrigNow = pendingKbdRetrig;
        pendingKbdRetrig = false;

        eg1.setGate(eg1Gate, retrigNow && !patchBay.isConnected(PatchDest::Eg1Trig_In));
        eg2.setGate(eg2Gate, retrigNow);

        const auto eg1Out = eg1.processSample();
        const auto eg2Out = eg2.processSample();

        patchSources[static_cast<size_t>(PatchSource::EG1_Out)]    = eg1Out.normal;
        patchSources[static_cast<size_t>(PatchSource::EG1_RevOut)] = eg1Out.reverse;
        patchSources[static_cast<size_t>(PatchSource::EG2_RevOut)] = eg2Out.reverse;

        // 6. Process Sample & Hold (J2 S/H In, J5 Clock In -> J8 S/H Out)
        // If J2 is unpatched, normal to Pink Noise; if J5 is unpatched, normal to MG Rectangle Out
        const double shIn = patchBay.read(PatchDest::SH_In, patchSources, noiseGen.getLastPink());
        const double shClock = patchBay.read(PatchDest::SH_ClockIn, patchSources, mgOut.rectOut);
        const double shOut = sampleAndHold.processSample(shIn, shClock);
        patchSources[static_cast<size_t>(PatchSource::SH_Out)] = shOut;

        // 7. Process Vactrol Modulation VCA (J10 MVCA In, J15 Control In -> J16 MVCA Out)
        // Normal J10 to MG Triangle Out so patching Control In (e.g. from Wheel or EG1) works immediately
        const double mvcaIn = patchBay.read(PatchDest::MVCA_In, patchSources, mgOut.triOut);
        const double mvcaCtrl = patchBay.read(PatchDest::MVCA_ControlIn, patchSources, 0.0);
        const double mvcaOut = mvca.processSample(mvcaIn, mvcaCtrl);
        patchSources[static_cast<size_t>(PatchSource::MVCA_Out)] = mvcaOut;

        // 8. Resolve Modulation Buses (respecting switching jacks J4, J6, J14, J21, J26, J24)
        // J4 TOTAL EXT replaces normalled MG Triangle Out on VCO, HPF, and LPF MG/T.EXT knobs!
        const double totalModSignal = patchBay.read(PatchDest::TotalExt_In, patchSources, mgOut.triOut);
        // J6 FREQ EXT replaces normalled EG-1 Normal Out on VCO EG1/EXT knob!
        const double freqModSignal = patchBay.read(PatchDest::FreqExt_In, patchSources, eg1Out.normal);
        // J14 HPF Cutoff Mod replaces normalled EG-2 Normal Out on HPF EG2/EXT knob!
        const double hpfEg2ModSignal = patchBay.read(PatchDest::HpfCutoff_In, patchSources, eg2Out.normal);
        // J21 LPF Cutoff Mod replaces normalled EG-2 Normal Out on LPF EG2/EXT knob!
        const double lpfEg2ModSignal = patchBay.read(PatchDest::LpfCutoff_In, patchSources, eg2Out.normal);

        // 9. Compute VCO 1 and VCO 2 Pitches
        // J26 VCO 1+2 CV IN replaces normalled glided KBD CV
        const double vco12BaseCvOct = patchBay.read(PatchDest::Vco12Cv_In, patchSources, glidedKbdCvOct);
        // J24 VCO 2 CV IN replaces normalled VCO 1+2 CV for VCO 2
        const double vco2BaseCvOct = patchBay.read(PatchDest::Vco2Cv_In, patchSources, vco12BaseCvOct);

        // Shared FM modulation in octaves (MG/T.EXT up to ±1.5 oct, EG1/EXT up to ±2.0 oct)
        const double fmOctaves = (p.fmMgExt * p.fmMgExt * 1.5) * totalModSignal
                               + (p.fmEg1Ext * p.fmEg1Ext * 2.0) * freqModSignal
                               + (p.masterTune / 1200.0);

        // VCO 1 Scale: 0=32' (-2 oct), 1=16' (-1 oct), 2=8' (0 oct), 3=4' (+1 oct)
        const double vco1ScaleOct = static_cast<double>(p.vco1Scale - 2);
        // VCO 2 Scale: 0=16' (-1 oct), 1=8' (0 oct), 2=4' (+1 oct), 3=2' (+2 oct)
        const double vco2ScaleOct = static_cast<double>(p.vco2Scale - 1);
        const double vco2DetuneOct = p.vco2Pitch / 12.0;

        // C2 reference frequency = 65.40639 Hz (MIDI note 36)
        const double freq1Hz = 65.40639 * std::pow(2.0, vco12BaseCvOct + vco1ScaleOct + fmOctaves);
        const double freq2Hz = 65.40639 * std::pow(2.0, vco2BaseCvOct + vco2ScaleOct + vco2DetuneOct + fmOctaves);

        // 10. Compute Korg-35 HPF and LPF Cutoff Frequencies
        // Hardware MS-20 cutoff knobs sweep 35 Hz to ~17,500 Hz (~9 octaves)
        // Modulation inputs operate at 2 OCT/V (so ±5V sweeps ±6..8 octaves!)
        const double hpfBaseOct = p.hpfCutoff * 8.8;
        const double hpfModOct  = (p.hpfMgExt * 4.5) * totalModSignal
                                + (p.hpfEg2Ext * 5.5) * hpfEg2ModSignal;
        const double targetHpfHz = 32.0 * std::pow(2.0, std::clamp(hpfBaseOct + hpfModOct, -1.0, 9.3));

        const double lpfBaseOct = p.lpfCutoff * 9.0;
        const double lpfModOct  = (p.lpfMgExt * 4.5) * totalModSignal
                                + (p.lpfEg2Ext * 6.0) * lpfEg2ModSignal;
        const double targetLpfHz = 35.0 * std::pow(2.0, std::clamp(lpfBaseOct + lpfModOct, -1.0, 9.4));

        smoothedHpfHz += paramSmoothCoeff * (targetHpfHz - smoothedHpfHz);
        smoothedLpfHz += paramSmoothCoeff * (targetLpfHz - smoothedLpfHz);

        hpf.setParameters(smoothedHpfHz, p.hpfPeak);
        lpf.setParameters(smoothedLpfHz, p.lpfPeak);

        // 11. Main VCA control voltage: normalled EG-2 + J22 INITIAL GAIN
        const double initGainCv = patchBay.read(PatchDest::InitialGain_In, patchSources, 0.0);
        const double vcaCv = std::clamp(eg2Out.normal + initGainCv, 0.0, 1.4);

        // External audio signal into VCO mixer (J13 EXT SIG IN)
        const double extSigIn = patchBay.read(PatchDest::ExtSig_In, patchSources, 0.0);

        // Mixer gains (100KA audio taper: above 0.65 drives the Korg-35 input hard!)
        const double gVco1 = p.vco1Level * p.vco1Level * 1.45;
        const double gVco2 = p.vco2Level * p.vco2Level * 1.45;

        // 12. 4x Oversampled Audio Loop (VCOs -> Mixer -> Korg35 VCHPF -> Korg35 VCLPF -> Discrete VCA)
        double downsampled = 0.0;
        double lastWhite = 0.0;
        double lastPink = 0.0;

        for (int os = 0; os < oversampleFactor; ++os)
        {
            noiseGen.processSample(lastWhite, lastPink);

            const auto vcoOut = vcos.processSample(
                freq1Hz, freq2Hz, p.vco1Wave, p.vco1Pw, p.vco2Wave, lastWhite);

            // Summing node into IC9 KORG-35 HPF
            const double mixerSum = vcoOut.vco1Out * gVco1
                                  + vcoOut.vco2Out * gVco2
                                  + extSigIn * 1.25;

            const double hpfOut = hpf.processSample(mixerSum);
            const double lpfOut = lpf.processSample(hpfOut);

            // Q14/Q15/Q16 discrete differential transistor VCA with subtle thermal saturation
            const double vcaSample = std::tanh(lpfOut * 0.85) * vcaCv;

            // 2-stage anti-aliasing lowpass filter before decimation
            aaState1 += 0.42 * (vcaSample - aaState1);
            aaState2 += 0.42 * (aaState1 - aaState2);
            downsampled = aaState2;
        }

        patchSources[static_cast<size_t>(PatchSource::WhiteNoise_Out)] = lastWhite;
        patchSources[static_cast<size_t>(PatchSource::PinkNoise_Out)]  = lastPink;

        // DC blocker (output coupling capacitor C33 10uF on KLM-127)
        const double acOut = downsampled - dcBlockX1 + dcBlockCoeff * dcBlockY1;
        dcBlockX1 = downsampled;
        dcBlockY1 = acOut;

        lastSignalOut = std::clamp(acOut, -1.5, 1.5);
        // IC3 4558 Headphone Amp (KLM-128) has hotter gain, prized for patching PHONES OUT -> EXT SIG IN!
        lastPhonesOut = std::tanh(acOut * 1.85) * 1.35;

        // Telemetry for front-panel LEDs (KBD TRIG LED, MG LED, ESP PEAK & TRIG LEDs)
        ledKbdTrig.store(eg2Gate, std::memory_order_relaxed);
        ledMgRate.store(mgOut.rectOut > 0.5, std::memory_order_relaxed);
        ledEspPeak.store(espOut.peakLed, std::memory_order_relaxed);
        ledEspTrig.store(espOut.trigLed, std::memory_order_relaxed);
        currentSignalMeter.store(static_cast<float>(std::abs(lastSignalOut)), std::memory_order_relaxed);

        return static_cast<float>(acOut * p.masterVolume * 0.85);
    }

    MS20PatchBay& getPatchBay() noexcept { return patchBay; }
    const MS20PatchBay& getPatchBay() const noexcept { return patchBay; }

    bool getKbdTrigLed() const noexcept { return ledKbdTrig.load(std::memory_order_relaxed); }
    bool getMgRateLed() const noexcept  { return ledMgRate.load(std::memory_order_relaxed); }
    bool getEspPeakLed() const noexcept { return ledEspPeak.load(std::memory_order_relaxed); }
    bool getEspTrigLed() const noexcept { return ledEspTrig.load(std::memory_order_relaxed); }
    float getSignalMeter() const noexcept { return currentSignalMeter.load(std::memory_order_relaxed); }
    int getCurrentMidiNote() const noexcept { return currentMidiNote; }
    bool isGateActive() const noexcept { return kbdGate; }

private:
    double hostSampleRate = 44100.0;
    double oversampledRate = 176400.0;

    MS20DualVCO vcos;
    NoiseGenerator noiseGen;
    Korg35Filter hpf;
    Korg35Filter lpf;
    MS20EnvelopeEG1 eg1;
    MS20EnvelopeEG2 eg2;
    MS20ModulationGenerator mg;
    MS20SampleAndHold sampleAndHold;
    MS20VactrolMVCA mvca;
    MS20ESP esp;
    MS20PatchBay patchBay;

    std::array<double, MS20PatchBay::numSources> patchSources {};
    std::array<bool, 128> heldNotes {};

    int currentMidiNote = 48; // C3 default
    bool kbdGate = false;
    bool pendingKbdRetrig = false;
    double lastVelocity = 1.0;
    double pitchBendSemitones = 0.0;
    double targetKbdCvOct = 1.0;
    double glidedKbdCvOct = 1.0;

    double smoothedHpfHz = 40.0;
    double smoothedLpfHz = 2500.0;
    double paramSmoothCoeff = 0.02;

    double lastSignalOut = 0.0;
    double lastPhonesOut = 0.0;
    double dcBlockX1 = 0.0;
    double dcBlockY1 = 0.0;
    double dcBlockCoeff = 0.999;
    double aaState1 = 0.0;
    double aaState2 = 0.0;

    std::atomic<bool> ledKbdTrig { false };
    std::atomic<bool> ledMgRate { false };
    std::atomic<bool> ledEspPeak { false };
    std::atomic<bool> ledEspTrig { false };
    std::atomic<float> currentSignalMeter { 0.0f };

    bool isAnyNoteHeld() const noexcept
    {
        for (bool h : heldNotes)
            if (h) return true;
        return false;
    }

    /**
     * KLM-127 Keyboard CV uses a constant-current series resistor chain where the
     * lowest pressed key shunts higher keys (Low-Note Priority).
     */
    void updateActiveNoteFromLadder() noexcept
    {
        for (int i = 0; i < 128; ++i)
        {
            if (heldNotes[static_cast<size_t>(i)])
            {
                currentMidiNote = i;
                return;
            }
        }
    }
};

} // namespace ms20::dsp
