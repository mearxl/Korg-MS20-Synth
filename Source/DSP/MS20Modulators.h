#pragma once

#include <cmath>
#include <algorithm>

namespace ms20::dsp
{

/**
 * KLM-127 Envelope Generator 1 (EG-1): Delay -> Attack -> Release
 * Uses true analog RC charge/discharge curves (C38 + TL082 + MC14007 switches).
 */
class MS20EnvelopeEG1
{
public:
    enum class Stage
    {
        Idle,
        Delay,
        Attack,
        Sustaining,
        Release
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        env = 0.0;
        delayElapsed = 0.0;
        gateHigh = false;
    }

    void setParameters(double delayNorm, double attackNorm, double releaseNorm) noexcept
    {
        // VR27, VR28, VR29 are 1MA (audio/log taper) pots: 0 to 10 seconds
        delaySeconds = potToTimeSeconds(delayNorm, 0.0, 10.0);
        attackSeconds = potToTimeSeconds(attackNorm, 0.0008, 10.0);
        releaseSeconds = potToTimeSeconds(releaseNorm, 0.0015, 10.0);

        attackCoeff = calcRcCoeff(attackSeconds);
        releaseCoeff = calcRcCoeff(releaseSeconds);
    }

    void setGate(bool newGate, bool retrigger) noexcept
    {
        if (retrigger && newGate)
        {
            delayElapsed = 0.0;
            stage = (delaySeconds > 0.001) ? Stage::Delay : Stage::Attack;
        }
        else if (newGate && !gateHigh)
        {
            delayElapsed = 0.0;
            stage = (delaySeconds > 0.001) ? Stage::Delay : Stage::Attack;
        }
        else if (!newGate && gateHigh)
        {
            stage = Stage::Release;
        }
        gateHigh = newGate;
    }

    struct Output
    {
        double normal = 0.0;  // 0.0 to +1.0 (0V to +5V)
        double reverse = 1.0; // +1.0 to 0.0 (+5V to 0V at J11)
    };

    inline Output processSample() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                env = 0.0;
                break;

            case Stage::Delay:
                delayElapsed += 1.0 / sampleRate;
                if (!gateHigh)
                {
                    stage = Stage::Release;
                }
                else if (delayElapsed >= delaySeconds)
                {
                    stage = Stage::Attack;
                }
                break;

            case Stage::Attack:
                // RC charge toward 1.22 asymptotic rail, comparator trips at 1.0 (+5V)
                env += attackCoeff * (1.22 - env);
                if (env >= 1.0)
                {
                    env = 1.0;
                    stage = gateHigh ? Stage::Sustaining : Stage::Release;
                }
                break;

            case Stage::Sustaining:
                env = 1.0;
                if (!gateHigh)
                    stage = Stage::Release;
                break;

            case Stage::Release:
                env += releaseCoeff * (-0.02 - env);
                if (env <= 0.0001)
                {
                    env = 0.0;
                    stage = Stage::Idle;
                }
                break;
        }

        return { env, 1.0 - env };
    }

private:
    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    double env = 0.0;
    double delayElapsed = 0.0;
    bool gateHigh = false;

    double delaySeconds = 0.0;
    double attackSeconds = 0.01;
    double releaseSeconds = 0.1;
    double attackCoeff = 0.01;
    double releaseCoeff = 0.01;

    static inline double potToTimeSeconds(double norm, double minSec, double maxSec) noexcept
    {
        const double clamped = std::clamp(norm, 0.0, 1.0);
        // 1MA logarithmic pot curve (fine resolution in 0..300ms range)
        return minSec + (maxSec - minSec) * (clamped * clamped * clamped);
    }

    inline double calcRcCoeff(double timeSec) const noexcept
    {
        const double samples = std::max(1.0, timeSec * sampleRate * 0.35);
        return 1.0 - std::exp(-1.0 / samples);
    }
};

/**
 * KLM-127 Envelope Generator 2 (EG-2): Hold -> Attack -> Decay -> Sustain -> Release
 * Uses true analog RC charge/discharge curves (C39/C42 + TL081 + MC14007).
 */
class MS20EnvelopeEG2
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        env = 0.0;
        holdRemaining = 0.0;
        gateHigh = false;
    }

    void setParameters(double holdNorm,
                       double attackNorm,
                       double decayNorm,
                       double sustainNorm,
                       double releaseNorm) noexcept
    {
        // VR34 Hold is 2MA pot up to 20s; Attack/Decay/Release are 1MA up to 10s; Sustain is 10KB linear
        holdSeconds = (holdNorm <= 0.002) ? 0.0 : (0.002 + 20.0 * (holdNorm * holdNorm * holdNorm));
        attackSeconds = 0.0006 + 10.0 * (attackNorm * attackNorm * attackNorm);
        decaySeconds = 0.0015 + 10.0 * (decayNorm * decayNorm * decayNorm);
        sustainLevel = std::clamp(sustainNorm, 0.0, 1.0);
        releaseSeconds = 0.0015 + 10.0 * (releaseNorm * releaseNorm * releaseNorm);

        attackCoeff = calcRcCoeff(attackSeconds);
        decayCoeff = calcRcCoeff(decaySeconds);
        releaseCoeff = calcRcCoeff(releaseSeconds);
    }

    void setGate(bool newGate, bool retrigger) noexcept
    {
        if (retrigger && newGate)
        {
            holdRemaining = holdSeconds;
            stage = Stage::Attack;
        }
        else if (newGate && !gateHigh)
        {
            holdRemaining = holdSeconds;
            stage = Stage::Attack;
        }
        else if (!newGate && gateHigh)
        {
            // On KLM-127 (C42 + IC23), Hold timer starts counting on trigger/gate release
            if (holdRemaining <= 0.0005)
                stage = Stage::Release;
        }
        gateHigh = newGate;
    }

    struct Output
    {
        double normal = 0.0;  // 0.0 to +1.0 (0V to +5V internal)
        double reverse = 0.0; // -1.0 to 0.0 (-5V to 0V at J23)
    };

    inline Output processSample() noexcept
    {
        // Update C42 Hold capacitor timer when physical gate is released
        if (!gateHigh && holdRemaining > 0.0)
        {
            holdRemaining -= 1.0 / sampleRate;
            if (holdRemaining <= 0.0)
            {
                holdRemaining = 0.0;
                stage = Stage::Release;
            }
        }

        const bool effectiveGate = gateHigh || (holdRemaining > 0.0);

        switch (stage)
        {
            case Stage::Idle:
                env = 0.0;
                break;

            case Stage::Attack:
                env += attackCoeff * (1.20 - env);
                if (env >= 1.0)
                {
                    env = 1.0;
                    stage = effectiveGate ? Stage::Decay : Stage::Release;
                }
                else if (!effectiveGate)
                {
                    stage = Stage::Release;
                }
                break;

            case Stage::Decay:
                env += decayCoeff * (sustainLevel - env);
                if (!effectiveGate)
                {
                    stage = Stage::Release;
                }
                else if (std::abs(env - sustainLevel) < 0.0005)
                {
                    env = sustainLevel;
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                env += decayCoeff * (sustainLevel - env);
                if (!effectiveGate)
                    stage = Stage::Release;
                break;

            case Stage::Release:
                env += releaseCoeff * (-0.015 - env);
                if (env <= 0.0001)
                {
                    env = 0.0;
                    stage = Stage::Idle;
                }
                break;
        }

        // J23 EG-2 Reverse output is -(1.0 - env) -> ranges from -1.0 (-5V at rest) to 0.0 (0V at peak)
        return { env, -(1.0 - env) };
    }

    bool isActive() const noexcept
    {
        return stage != Stage::Idle || env > 0.0001;
    }

private:
    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    double env = 0.0;
    double holdRemaining = 0.0;
    bool gateHigh = false;

    double holdSeconds = 0.0;
    double attackSeconds = 0.005;
    double decaySeconds = 0.2;
    double sustainLevel = 0.7;
    double releaseSeconds = 0.2;

    double attackCoeff = 0.01;
    double decayCoeff = 0.01;
    double releaseCoeff = 0.01;

    inline double calcRcCoeff(double timeSec) const noexcept
    {
        const double samples = std::max(1.0, timeSec * sampleRate * 0.35);
        return 1.0 - std::exp(-1.0 / samples);
    }
};

/**
 * KLM-127 Modulation Generator (MG): IC14 & IC15 4558 relaxation oscillator
 * with VR26 (1MB) skew control morphing continuously between Saw, Triangle, and Ramp
 * on the Triangle Out (J3) and Wide Pulse, Square, and Narrow Pulse on Rectangle Out (J7).
 */
class MS20ModulationGenerator
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0;
    }

    struct Output
    {
        double triOut = 0.0;  // -1.0 to +1.0 (5Vp-p bipolar, J3 & normalled bus)
        double rectOut = 0.0; // 0.0 to +1.0 (0V to +5V unipolar pulse, J7 & LED)
    };

    /**
     * @param shapeNorm  VR26 Waveform knob (0.0 = fall saw, 0.5 = symmetric triangle, 1.0 = rise saw)
     * @param rateNorm   VR25 Frequency knob (0.1 Hz to 25 Hz exponential)
     */
    inline Output processSample(double shapeNorm, double rateNorm) noexcept
    {
        // VR25 1MA pot: 0.1 Hz to 25 Hz
        const double freqHz = 0.1 * std::pow(250.0, std::clamp(rateNorm, 0.0, 1.0));
        const double dt = freqHz / sampleRate;

        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        // Skew ratio from 1:80 (0.015) through 1:1 (0.50) to 80:1 (0.985)
        const double skew = std::clamp(0.015 + shapeNorm * 0.97, 0.015, 0.985);

        double tri = 0.0;
        double rect = 0.0;
        if (phase < skew)
        {
            // Rising segment of integrator IC14
            tri = -1.0 + 2.0 * (phase / skew);
            rect = 1.0;
        }
        else
        {
            // Falling segment of integrator IC14
            tri = 1.0 - 2.0 * ((phase - skew) / (1.0 - skew));
            rect = 0.0;
        }

        // Subtle op-amp integrator rounding at turnaround peaks
        tri = std::tanh(tri * 1.15) / std::tanh(1.15);

        lastTri = tri;
        lastRect = rect;
        return { tri, rect };
    }

    double getLastTri() const noexcept { return lastTri; }
    double getLastRect() const noexcept { return lastRect; }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
    double lastTri = 0.0;
    double lastRect = 0.0;
};

/**
 * KLM-128 Sample & Hold (IC1 CA3140E MOSFET op-amp + F1 2SK30A FET switch)
 */
class MS20SampleAndHold
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        slewCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 2500.0 / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        heldValue = 0.0;
        smoothedOut = 0.0;
        prevClockHigh = false;
    }

    inline double processSample(double signalIn, double clockIn) noexcept
    {
        // Clock input J5 has diode D1 + comparator IC2 (4558) triggering on positive edge / gate high
        const bool clockHigh = (clockIn > 0.25);
        if (clockHigh && !prevClockHigh)
        {
            heldValue = std::clamp(signalIn, -1.5, 1.5);
        }
        prevClockHigh = clockHigh;

        // Very slight capacitor droop on C1 + 2SK30A acquisition smoothing
        heldValue *= 0.999998;
        smoothedOut += slewCoeff * (heldValue - smoothedOut);
        lastOut = smoothedOut;
        return smoothedOut;
    }

    double getLastOut() const noexcept { return lastOut; }

private:
    double sampleRate = 44100.0;
    double heldValue = 0.0;
    double smoothedOut = 0.0;
    double slewCoeff = 0.2;
    double lastOut = 0.0;
    bool prevClockHigh = false;
};

/**
 * KLM-128 Modulation VCA (MVCA): IC3 4558 + TR1 2SC1685 + PC1 HTV-P873-G35-201B Photocoupler
 * The CdS vactrol gives the MS-20's patch-bay VCA its signature organic response!
 */
class MS20VactrolMVCA
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        // HTV-P873 photocoupler attack (~2.5ms) and decay (~18ms) time constants
        attackCoeff = 1.0 - std::exp(-1.0 / (0.0025 * sampleRate));
        releaseCoeff = 1.0 - std::exp(-1.0 / (0.0180 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        vactrolGain = 0.0;
        lastOut = 0.0;
    }

    inline double processSample(double signalIn, double controlIn) noexcept
    {
        // Control voltage J15 (0V to +5V -> 0.0 to 1.0) drives TR1 LED current
        const double targetGain = std::clamp(controlIn, 0.0, 1.5);
        const double coeff = (targetGain > vactrolGain) ? attackCoeff : releaseCoeff;
        vactrolGain += coeff * (targetGain - vactrolGain);

        lastOut = signalIn * vactrolGain;
        return lastOut;
    }

    double getLastOut() const noexcept { return lastOut; }

private:
    double sampleRate = 44100.0;
    double vactrolGain = 0.0;
    double attackCoeff = 0.01;
    double releaseCoeff = 0.002;
    double lastOut = 0.0;
};

} // namespace ms20::dsp
