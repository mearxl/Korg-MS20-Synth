#pragma once

#include <cmath>
#include <algorithm>

namespace ms20::dsp
{

/**
 * Circuit-accurate model of the KLM-129 External Signal Processor (ESP)
 * Includes:
 *   - IC7 4558 Preamp (VR7 Signal Level) -> AMP OUT (J9) & PEAK LED
 *   - IC6 4558 2-pole High-Pass (VR6 Low Cut 50..2500Hz) + 2-pole Low-Pass (VR5 High Cut 100..5000Hz) -> FILTER OUT (J17)
 *   - IC5/IC2/IC4/IC3 Zero-Crossing Charge-Pump Frequency-to-Voltage Converter (VR4 CV Adjust) -> F-V CV OUT (J18)
 *   - IC1 Precision Rectifier Envelope Follower -> ENV OUT (J29)
 *   - Comparator Trigger Detector (VR3 Threshold Level) -> ESP TRIG OUT (J30) & TRIG LED
 */
class MS20ESP
{
public:
    struct Output
    {
        double ampOut = 0.0;      // J9 AMP OUT (-1.0 to +1.0 nominal)
        double filterOut = 0.0;   // J17 BAND PASS FILTERED OUT
        double fvCvOut = 0.0;     // J18 F->V CV OUT (pitch CV in octaves relative to C2 = 65.41Hz, or Hz/V normalized)
        double envOut = 0.0;      // J29 ENV OUT (0.0 to +1.0 -> 0V to +5V)
        double trigOut = 0.0;     // J30 ESP TRIG OUT (1.0 when triggered, 0.0 when idle)
        bool peakLed = false;
        bool trigLed = false;
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        // Envelope follower attack (~1.8ms via R3/D4 into C2 1uF) and release (~45ms via R2 100K)
        envAttackCoeff = 1.0 - std::exp(-1.0 / (0.0018 * sampleRate));
        envReleaseCoeff = 1.0 - std::exp(-1.0 / (0.0450 * sampleRate));
        // F-V integrator smoothing (C6 0.056uF + 2SK30 sample/hold)
        fvSmoothCoeff = 1.0 - std::exp(-1.0 / (0.0080 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        hp1 = hp2 = 0.0;
        lp1 = lp2 = 0.0;
        envState = 0.0;
        samplesSinceCrossing = 0;
        prevFilterSample = 0.0;
        trackedFreqHz = 110.0;
        smoothedCvOct = 0.0;
        trigGate = false;
        lastOut = {};
    }

    void setParameters(double sigLevelNorm,
                       double lowCutNorm,
                       double highCutNorm,
                       double cvAdjustNorm,
                       double thresholdNorm) noexcept
    {
        // VR7 Signal Level (1MB audio taper: 0.0 up to 25x gain so guitar/mic/synth levels can overdrive IC7)
        preampGain = (sigLevelNorm <= 0.001) ? 0.0 : (0.15 * sigLevelNorm + 18.0 * sigLevelNorm * sigLevelNorm * sigLevelNorm);

        // VR6 Low Cut (50 Hz to 2,500 Hz)
        const double lowCutHz = 50.0 * std::pow(50.0, std::clamp(lowCutNorm, 0.0, 1.0));
        // VR5 High Cut (100 Hz to 5,000 Hz)
        const double highCutHz = 100.0 * std::pow(50.0, std::clamp(highCutNorm, 0.0, 1.0));

        gHp = std::tan(3.14159265358979323846 * std::clamp(lowCutHz, 20.0, sampleRate * 0.42) / sampleRate);
        gLp = std::tan(3.14159265358979323846 * std::clamp(highCutHz, 40.0, sampleRate * 0.42) / sampleRate);

        // VR4 CV Adjust (scales F-V output around unity pitch tracking at 0.5..0.7)
        cvAdjust = std::clamp(cvAdjustNorm, 0.0, 1.0) * 1.6;

        // VR3 Threshold Level (10KB linear: 0.02 to 0.95)
        threshold = 0.02 + std::clamp(thresholdNorm, 0.0, 1.0) * 0.88;
    }

    inline Output processSample(double externalInput) noexcept
    {
        // 1. IC7 4558 Preamp stage (with op-amp rail saturation at ±12V equivalent)
        const double rawAmp = externalInput * preampGain;
        const double ampOut = std::tanh(rawAmp * 0.85) / 0.85;
        const bool peakLed = std::abs(rawAmp) > 0.92;

        // 2. IC1 Envelope Follower (Precision half-wave rectifier + C2 1uF filter)
        const double rect = std::abs(ampOut);
        const double envCoeff = (rect > envState) ? envAttackCoeff : envReleaseCoeff;
        envState += envCoeff * (rect - envState);
        const double envOut = std::clamp(envState * 1.35, 0.0, 1.25);

        // 3. IC1 Comparator Trigger Detector (with hysteresis via R20 680K)
        const double threshOn = threshold;
        const double threshOff = threshold * 0.78;
        if (!trigGate && envOut >= threshOn)
            trigGate = true;
        else if (trigGate && envOut < threshOff)
            trigGate = false;

        // 4. IC6 Bandpass Filter (2-pole Low Cut HPF + 2-pole High Cut LPF)
        const double GHp = 1.0 / (1.0 + gHp);
        const double hpStage1 = (ampOut - hp1) * GHp;
        hp1 = 2.0 * (ampOut - hpStage1) - hp1;
        const double hpStage2 = (hpStage1 - hp2) * GHp;
        hp2 = 2.0 * (hpStage1 - hpStage2) - hp2;

        const double GLp = gLp / (1.0 + gLp);
        const double lpStage1 = GLp * hpStage2 + lp1 / (1.0 + gLp);
        lp1 = 2.0 * lpStage1 - lp1;
        const double lpStage2 = GLp * lpStage1 + lp2 / (1.0 + gLp);
        lp2 = 2.0 * lpStage2 - lp2;

        const double filterOut = lpStage2;

        // 5. IC5/IC2/IC4/IC3 Zero-Crossing Frequency-to-Voltage Converter
        ++samplesSinceCrossing;
        if (prevFilterSample <= 0.0 && filterOut > 0.0 && envOut > 0.03)
        {
            if (samplesSinceCrossing > 8)
            {
                const double instFreq = sampleRate / static_cast<double>(samplesSinceCrossing);
                if (instFreq >= 35.0 && instFreq <= 4500.0)
                {
                    trackedFreqHz = instFreq;
                }
            }
            samplesSinceCrossing = 0;
        }
        prevFilterSample = filterOut;

        // Convert tracked frequency to pitch CV (octaves relative to C2 = 65.406Hz, scaled by VR4 CV Adjust)
        const double rawOct = std::log2(std::max(32.7, trackedFreqHz) / 65.40639) * cvAdjust;
        smoothedCvOct += fvSmoothCoeff * (rawOct - smoothedCvOct);

        lastOut = {
            ampOut,
            filterOut,
            std::clamp(smoothedCvOct, -2.0, 5.0),
            std::clamp(envOut, 0.0, 1.0),
            trigGate ? 1.0 : 0.0,
            peakLed,
            trigGate
        };
        return lastOut;
    }

    const Output& getLastOutput() const noexcept { return lastOut; }

private:
    double sampleRate = 44100.0;
    double preampGain = 1.0;
    double gHp = 0.01;
    double gLp = 0.2;
    double cvAdjust = 1.0;
    double threshold = 0.25;

    double hp1 = 0.0, hp2 = 0.0;
    double lp1 = 0.0, lp2 = 0.0;
    double envState = 0.0;
    double envAttackCoeff = 0.01;
    double envReleaseCoeff = 0.001;
    double fvSmoothCoeff = 0.005;

    int samplesSinceCrossing = 0;
    double prevFilterSample = 0.0;
    double trackedFreqHz = 110.0;
    double smoothedCvOct = 0.0;
    bool trigGate = false;
    Output lastOut {};
};

} // namespace ms20::dsp
