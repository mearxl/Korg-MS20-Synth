#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace ms20::dsp
{

/**
 * Hardware-modeled Noise Generator (KLM-128 TR2/TR3 2SC644R reverse-biased BJT shot noise
 * + IC4 4558 amplifier and R30/C5/C6/R32/C7 Pink Noise filter network).
 */
class NoiseGenerator
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        // High-pass coupling cap C5 (0.022uF into 100K) & ultrasonic roll-off C7 (47pF across 47K)
        const double hpCut = 18.0;
        hpCoeff = std::exp(-2.0 * 3.14159265358979323846 * hpCut / sampleRate);
        const double lpCut = std::min(16500.0, sampleRate * 0.42);
        lpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * lpCut / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0;
        whiteLp = 0.0;
        hpState = 0.0;
        lastWhite = 0.0;
        lastPink = 0.0;
    }

    inline void processSample(double& whiteOut, double& pinkOut) noexcept
    {
        // High-period xorshift64* + soft transistor shot-noise crest shaping
        rngState ^= rngState >> 12;
        rngState ^= rngState << 25;
        rngState ^= rngState >> 27;
        const uint64_t r = rngState * 2685821657736338717ULL;
        const double raw = (static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0)) * 2.0 - 1.0;

        // IC4 4558 bandwidth & C7 47pF rolloff
        whiteLp += lpCoeff * (raw - whiteLp);
        const double acWhite = whiteLp - hpState;
        hpState = whiteLp * (1.0 - hpCoeff) + hpState * hpCoeff;

        // KLM-128 Pink Noise RC ladder (-3dB/octave Paul Kellet / KLM-128 network)
        b0 = 0.99886 * b0 + acWhite * 0.0555179;
        b1 = 0.99332 * b1 + acWhite * 0.0750759;
        b2 = 0.96900 * b2 + acWhite * 0.1538520;
        b3 = 0.86650 * b3 + acWhite * 0.3104856;
        b4 = 0.55000 * b4 + acWhite * 0.5329522;
        b5 = -0.7616 * b5 - acWhite * 0.0168980;
        const double pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + acWhite * 0.5362) * 0.22;
        b6 = acWhite * 0.115926;

        lastWhite = acWhite;
        lastPink = pink;
        whiteOut = lastWhite;
        pinkOut = lastPink;
    }

    double getLastWhite() const noexcept { return lastWhite; }
    double getLastPink() const noexcept { return lastPink; }

private:
    double sampleRate = 176400.0;
    uint64_t rngState = 0x853C49E6748FEA9BULL;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, b3 = 0.0, b4 = 0.0, b5 = 0.0, b6 = 0.0;
    double whiteLp = 0.0;
    double hpState = 0.0;
    double hpCoeff = 0.999;
    double lpCoeff = 0.4;
    double lastWhite = 0.0;
    double lastPink = 0.0;
};

/**
 * Circuit-modeled VCO 1 & VCO 2 (KLM-127) with PolyBLEP anti-aliasing, capacitive saw core
 * discharge curvature, D2/D3/Q2 triangle shaper, and MC14007 CMOS Ring Modulator.
 */
class MS20DualVCO
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        // Subtle thermal drift LFO coefficients
        driftCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 0.35 / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        phase1 = 0.0;
        phase2 = 0.27; // Free-running phase offset like hardware
        drift1 = 0.0;
        drift2 = 0.0;
        acState1 = 0.0;
        acState2 = 0.0;
    }

    struct Output
    {
        double vco1Out = 0.0;
        double vco2Out = 0.0;
        double vco1PulseForRing = 0.0;
        double vco2SquareForRing = 0.0;
    };

    /**
     * @param freq1Hz      VCO 1 fundamental frequency in Hz
     * @param freq2Hz      VCO 2 fundamental frequency in Hz
     * @param vco1Wave     0 = Triangle, 1 = Saw, 2 = Pulse (Variable PW), 3 = White Noise
     * @param vco1PwKnob   VR9 Pulse Width knob (0.0 = 50% 1:1, 1.0 = narrow pulse -> 1:inf)
     * @param vco2Wave     0 = Saw, 1 = Square (50%), 2 = Narrow Pulse (18%), 3 = Ring Mod
     * @param whiteNoise   White noise sample from KLM-128 NoiseGenerator
     */
    inline Output processSample(double freq1Hz,
                                double freq2Hz,
                                int vco1Wave,
                                double vco1PwKnob,
                                int vco2Wave,
                                double whiteNoise) noexcept
    {
        // Subtle independent analog thermal drift (~ ±1.2 cents)
        driftRng ^= driftRng >> 12;
        driftRng ^= driftRng << 25;
        driftRng ^= driftRng >> 27;
        const double noiseSmall = (static_cast<double>((driftRng * 2685821657736338717ULL) >> 11) * (1.0 / 9007199254740992.0)) * 2.0 - 1.0;
        drift1 += driftCoeff * (noiseSmall * 0.0007 - drift1);
        drift2 += driftCoeff * (-noiseSmall * 0.00085 - drift2);

        const double f1 = std::clamp(freq1Hz * (1.0 + drift1), 5.0, sampleRate * 0.45);
        const double f2 = std::clamp(freq2Hz * (1.0 + drift2), 5.0, sampleRate * 0.45);

        const double dt1 = f1 / sampleRate;
        const double dt2 = f2 / sampleRate;

        phase1 += dt1;
        if (phase1 >= 1.0)
            phase1 -= 1.0;

        phase2 += dt2;
        if (phase2 >= 1.0)
            phase2 -= 1.0;

        // --- VCO 1 CORE (C3 1000pF sawtooth core + Q2/D2/D3 waveshapers) ---
        double rawSaw1 = 2.0 * phase1 - 1.0;
        rawSaw1 -= polyBlep(phase1, dt1);
        // Subtle capacitive RC sag from C3 / R10
        const double shapedSaw1 = rawSaw1 - 0.065 * (rawSaw1 * rawSaw1 - 0.333);

        // VCO 1 Pulse Width: VR9 sweeps duty cycle from 0.50 (1:1) down to 0.03 (near 1:inf)
        const double pw1 = std::clamp(0.50 - vco1PwKnob * 0.47, 0.025, 0.50);
        double pulse1 = (phase1 < pw1) ? 1.0 : -1.0;
        pulse1 += polyBlep(phase1, dt1);
        pulse1 -= polyBlep( std::fmod(phase1 - pw1 + 1.0, 1.0), dt1 );
        // DC balance pulse wave so volume stays punchy while retaining pulse asymmetry
        const double pulse1Ac = pulse1 - (2.0 * pw1 - 1.0) * 0.75;

        // VCO 1 Triangle: folded from sawtooth via D2/D3 1S-1555 diodes & Q2 2SC1685 on KLM-127
        // Produces a warm triangle with the authentic slight MS-20 fold-point glitch/harmonic
        double tri1 = 1.0 - 2.0 * std::abs(rawSaw1);
        tri1 = std::tanh(tri1 * 1.12) / std::tanh(1.12) + 0.025 * shapedSaw1;

        double vco1Signal = 0.0;
        switch (vco1Wave)
        {
            case 0: vco1Signal = tri1; break;
            case 1: vco1Signal = shapedSaw1; break;
            case 2: vco1Signal = pulse1Ac; break;
            case 3: vco1Signal = whiteNoise * 1.15; break;
            default: vco1Signal = shapedSaw1; break;
        }

        // --- VCO 2 CORE (C9 1000pF sawtooth core + IC6 4558 pulse shaper + IC8 MC14007 Ring Mod) ---
        double rawSaw2 = 2.0 * phase2 - 1.0;
        rawSaw2 -= polyBlep(phase2, dt2);
        const double shapedSaw2 = rawSaw2 - 0.065 * (rawSaw2 * rawSaw2 - 0.333);

        // VCO 2 Square (50% duty cycle, used both for mode 1 and Ring Modulator input)
        double sq2 = (phase2 < 0.5) ? 1.0 : -1.0;
        sq2 += polyBlep(phase2, dt2);
        sq2 -= polyBlep(std::fmod(phase2 + 0.5, 1.0), dt2);

        // VCO 2 Narrow Pulse (~18% duty cycle determined by R59/R60 threshold on IC6)
        constexpr double narrowPw = 0.18;
        double narrow2 = (phase2 < narrowPw) ? 1.0 : -1.0;
        narrow2 += polyBlep(phase2, dt2);
        narrow2 -= polyBlep(std::fmod(phase2 - narrowPw + 1.0, 1.0), dt2);
        narrow2 -= (2.0 * narrowPw - 1.0) * 0.75;

        // VCO 1 50% Square/Pulse tap feeding IC7/IC8 MC14007 CMOS XOR Ring Modulator
        // Note: on KLM-127 (page 4), the ring modulator receives VCO 1's pulse output (affected by PW knob VR9!)
        // and VCO 2's square output into IC8 MC14007! That means adjusting VCO 1's PW knob changes the timbre
        // of the Ring Modulator, just like on the real MS-20!
        const double ringIn1 = std::tanh(pulse1 * 2.2);
        const double ringIn2 = std::tanh(sq2 * 2.2);
        // MC14007 CMOS balanced switch + 4558 buffer with slight carrier feedthrough
        const double ringMod = -(ringIn1 * ringIn2) + 0.035 * (ringIn1 - ringIn2);

        double vco2Signal = 0.0;
        switch (vco2Wave)
        {
            case 0: vco2Signal = shapedSaw2; break;
            case 1: vco2Signal = sq2; break;
            case 2: vco2Signal = narrow2; break;
            case 3: vco2Signal = ringMod; break;
            default: vco2Signal = shapedSaw2; break;
        }

        return { vco1Signal, vco2Signal, pulse1, sq2 };
    }

private:
    double sampleRate = 176400.0;
    double phase1 = 0.0;
    double phase2 = 0.27;
    double drift1 = 0.0;
    double drift2 = 0.0;
    double driftCoeff = 0.00001;
    double acState1 = 0.0;
    double acState2 = 0.0;
    uint64_t driftRng = 0x243F6A8885A308D3ULL;

    static inline double polyBlep(double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }
};

} // namespace ms20::dsp
