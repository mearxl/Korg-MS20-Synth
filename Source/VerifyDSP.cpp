#include "DSP/MS20SynthEngine.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>

using namespace ms20::dsp;

static double computeRMS(const std::vector<float>& buf)
{
    double sum = 0.0;
    for (float x : buf)
        sum += static_cast<double>(x) * static_cast<double>(x);
    return std::sqrt(sum / static_cast<double>(buf.size()));
}

static double estimateZeroCrossingFreq(const std::vector<float>& buf, double sr)
{
    int crossings = 0;
    int firstCross = -1;
    int lastCross = -1;
    for (size_t i = 1; i < buf.size(); ++i)
    {
        if (buf[i - 1] <= 0.0f && buf[i] > 0.0f)
        {
            if (firstCross < 0)
                firstCross = static_cast<int>(i);
            lastCross = static_cast<int>(i);
            ++crossings;
        }
    }
    if (crossings < 2 || lastCross <= firstCross)
        return 0.0;
    const double avgPeriod = static_cast<double>(lastCross - firstCross) / static_cast<double>(crossings - 1);
    return sr / avgPeriod;
}

int main()
{
    constexpr double sr = 48000.0;
    MS20SynthEngine engine;
    engine.prepare(sr);

    // 1. Test Korg-35 LPF Self-Oscillation (Service Manual Section 7-4 VCF Fc Adjustment)
    {
        engine.reset();
        MS20Params p;
        p.vco1Level = 0.0;
        p.vco2Level = 0.0;
        p.hpfCutoff = 0.0;
        p.hpfPeak   = 0.0;
        p.lpfCutoff = 0.42; // ~640 Hz
        p.lpfPeak   = 1.0;  // Peak at "10" -> self-oscillation
        p.eg2Attack = 0.0;
        p.eg2Sustain = 1.0;

        engine.noteOn(60, 1.0f);
        std::vector<float> out(24000);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = engine.processSample(p);

        const std::vector<float> tail(out.begin() + 6000, out.end());
        const double rms = computeRMS(tail);
        const double freq = estimateZeroCrossingFreq(tail, sr);
        std::cout << "[TEST 1] Korg-35 LPF Self-Oscillation: RMS=" << std::fixed << std::setprecision(4)
                  << rms << ", Freq=" << freq << " Hz\n";
        assert(rms > 0.15 && rms < 1.5);
        assert(freq > 200.0 && freq < 1500.0);
    }

    // 2. Test Korg-35 HPF Self-Oscillation (Service Manual Section 7-4.1)
    {
        engine.reset();
        MS20Params p;
        p.vco1Level = 0.0;
        p.vco2Level = 0.0;
        p.hpfCutoff = 0.45; // ~500-700 Hz
        p.hpfPeak   = 1.0;  // Peak at "10" -> self-oscillation
        p.lpfCutoff = 1.0;  // LPF wide open
        p.lpfPeak   = 0.0;
        p.eg2Attack = 0.0;
        p.eg2Sustain = 1.0;

        engine.noteOn(60, 1.0f);
        std::vector<float> out(24000);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = engine.processSample(p);

        const std::vector<float> tail(out.begin() + 6000, out.end());
        const double rms = computeRMS(tail);
        const double freq = estimateZeroCrossingFreq(tail, sr);
        std::cout << "[TEST 2] Korg-35 HPF Self-Oscillation: RMS=" << rms << ", Freq=" << freq << " Hz\n";
        assert(rms > 0.15 && rms < 1.5);
        assert(freq > 200.0 && freq < 1500.0);
    }

    // 3. Test VCO 1 & VCO 2 + CMOS Ring Modulator + Patch Bay Sample & Hold
    {
        engine.reset();
        MS20Params p;
        p.vco1Wave = 1; // Saw
        p.vco2Wave = 3; // Ring Mod
        p.vco2Pitch = 7.0;
        p.vco1Level = 0.8;
        p.vco2Level = 0.8;
        p.lpfCutoff = 0.65;
        p.lpfPeak   = 0.70;

        engine.getPatchBay().connect(PatchSource::PinkNoise_Out, PatchDest::SH_In);
        engine.getPatchBay().connect(PatchSource::MG_RectOut,    PatchDest::SH_ClockIn);
        engine.getPatchBay().connect(PatchSource::SH_Out,        PatchDest::TotalExt_In);
        p.lpfMgExt = 0.6;
        p.mgFreq   = 0.75;

        engine.noteOn(48, 1.0f);
        std::vector<float> out(24000);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = engine.processSample(p);

        const double rms = computeRMS(out);
        std::cout << "[TEST 3] Dual VCO + CMOS Ring Mod + S/H Patch Bay: RMS=" << rms << "\n";
        assert(rms > 0.10 && rms < 1.5);
        engine.getPatchBay().clearAll();
    }

    // 4. Test External Signal Processor (ESP - KLM-129) Pitch & Trigger Tracking
    {
        engine.reset();
        MS20Params p;
        p.espSigLevel  = 0.75;
        p.espLowCut    = 0.10;
        p.espHighCut   = 0.85;
        p.espCvAdjust  = 0.625;
        p.espThreshold = 0.25;

        engine.getPatchBay().connect(PatchSource::ESP_FvCvOut, PatchDest::Vco12Cv_In);
        engine.getPatchBay().connect(PatchSource::ESP_TrigOut, PatchDest::Eg12Trig_In);

        std::vector<float> out(24000);
        bool trigObserved = false;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const double extSine220 = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 220.0 * static_cast<double>(i) / sr);
            out[i] = engine.processSample(p, extSine220);
            if (engine.getEspTrigLed())
                trigObserved = true;
        }
        const std::vector<float> tail(out.begin() + 6000, out.end());
        const double rms = computeRMS(tail);
        std::cout << "[TEST 4] ESP Pitch & Trigger Follower (220Hz external input): RMS=" << rms
                  << ", ESP Trig LED=" << (trigObserved ? "ON" : "OFF") << "\n";
        assert(trigObserved);
        assert(rms > 0.08);
    }

    std::cout << "ALL KORG MS-20 CIRCUIT DSP TESTS PASSED!\n";
    return 0;
}
