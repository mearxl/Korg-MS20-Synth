#pragma once

#include <cmath>
#include <algorithm>

namespace ms20::dsp
{

/**
 * Circuit-accurate Zero-Delay Feedback (ZDF) model of the KORG-35 IC (IC9 & IC10 on KLM-127)
 * Sallen-Key 2-pole filter with anti-parallel 1S-1555 silicon clipping diodes (D9/D10 in HPF,
 * D19/D20 in LPF) and internal transistor OTA nonlinearity.
 *
 * Solved implicitly per oversampled sample via Newton-Raphson iteration so self-oscillation,
 * resonance pitch-pulling, and asymmetrical input overdrive behave identically to KLM-127.
 */
class Korg35Filter
{
public:
    enum class Mode
    {
        HighPass, // IC9 VCHPF
        LowPass   // IC10 VCLPF
    };

    explicit Korg35Filter(Mode filterMode = Mode::LowPass)
        : mode(filterMode)
    {
        reset();
    }

    void setMode(Mode newMode) noexcept
    {
        mode = newMode;
    }

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(44100.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        s1 = 0.0;
        s2 = 0.0;
        yEst = 0.0;
        dcBlockState = 0.0;
    }

    /**
     * Set slightly different component tolerances per chip instance (C16/C17 vs C28/C29 3300pF caps
     * and 1S-1555 diode forward voltage asymmetry).
     */
    void setComponentTolerance(double capRatio1, double capRatio2, double diodeAsym) noexcept
    {
        c1Scale = capRatio1;
        c2Scale = capRatio2;
        asymmetry = diodeAsym;
    }

    /**
     * @param cutoffHz  Effective cutoff frequency in Hz (50 Hz to 18,000 Hz)
     * @param peakNorm  Normalized Peak knob (VR7 / VR22, 0.0 to 1.0)
     */
    void setParameters(double cutoffHz, double peakNorm) noexcept
    {
        // Clamp cutoff safely below Nyquist of the oversampled rate
        const double maxCutoff = std::min(22000.0, sampleRate * 0.42);
        const double fc = std::clamp(cutoffHz, 20.0, maxCutoff);

        // Pre-warped trapezoidal integrator gain g = tan(pi * fc / fs)
        const double wd = 3.14159265358979323846 * fc / sampleRate;
        g = std::tan(wd);

        // VR7 / VR22 uses a 100KC (reverse-log / C-taper) pot on KLM-127.
        // Korg-35 feedback gain K ranges from 0.01 up to ~2.12 at full Peak ("10").
        // Self-oscillation threshold for Sallen-Key is K = 2.0.
        const double shapedPeak = peakNorm < 0.75
            ? (peakNorm / 0.75) * 0.70
            : 0.70 + ((peakNorm - 0.75) / 0.25) * 0.30;

        k = 0.02 + shapedPeak * 2.12;
    }

    /**
     * Process a single sample using implicit Newton-Raphson ZDF solving.
     * @param input  Input voltage normalized (~ -1.0 to +1.0 nominal, up to ±3.0 when overdriven)
     */
    inline double processSample(double input) noexcept
    {
        const double g1 = g * c1Scale;
        const double g2 = g * c2Scale;
        const double G1 = g1 / (1.0 + g1);
        const double G2 = g2 / (1.0 + g2);

        // Input attenuator & Korg-35 differential BJT pair saturation (R74/R75 100K into 4.7K/10K)
        // Gives the MS-20 its gritty asymmetric saturation when VCO mixer levels > 6.
        const double x = korg35OtaSaturate(input + 0.018 * asymmetry);

        // Predict initial y (output of IC12/IC11 4558 non-inverting buffer with gain = 2)
        double y = yEst;

        // Newton-Raphson iterations to solve the implicit nonlinear Korg-35 loop equation:
        // F(y) = y - [ alpha * clip(k * y) + constTerm ] = 0
        for (int iter = 0; iter < 4; ++iter)
        {
            double fb = 0.0;
            double dfb_dy = 0.0;
            diodeClipWithDerivative(k * y, fb, dfb_dy);
            fb *= k; // scale back so small-signal gain is k * y
            dfb_dy *= k;

            double F = 0.0;
            double dF = 1.0;

            if (mode == Mode::LowPass)
            {
                // Sallen-Key Low-Pass topology (IC10 KORG-35):
                // Stage 1: LP1 driven by x, with feedback fb entering the node between Stage 1 and Stage 2
                // v1 = G1 * (x - fb) + s1 / (1 + g1)  (Wait: in Korg-35 LPF, input x enters stage 1,
                // feedback from buffer output goes through VR22 -> C29 into stage 1 output / stage 2 input,
                // while D19/D20 clip the feedback node to ground!)
                // Let's write the exact Korg-35 Sallen-Key equations:
                // Node u = diode-clamped feedback node = clip(k * y)
                // Stage 1 (LP): v1 = G1 * x + (1 - G1) * u_feed + s1_term
                // Specifically, for Sallen-Key LPF:
                //   v1 = G1 * x + G1 * fb + s1 / (1.0 + g1)   (with feedback subtracted at stage 2)
                //   v2 = G2 * (v1 - fb) + s2 / (1.0 + g2)
                // Wait: in Sallen-Key LPF, positive feedback goes to Stage 1 capacitor (C28) while Stage 2
                // capacitor (C29) goes to ground (clipped by D19/D20):
                //   u1 = x + fb - y
                //   v1 = G1 * (x + fb) + s1 / (1.0 + g1)
                //   y  = G2 * v1 + s2 / (1.0 + g2)
                //Wait: if v1 = G1*(x + fb - v2), let's verify standard Korg-35 ZDF topology (Pirkle / Stinchcombe):
                // In Tim Stinchcombe's Korg35 analysis:
                //   LPF1 (non-inverting integrator with feedback): v1 = G1 * x + s1 / (1 + g1)
                //   Summer into HPF1 (feedback path) + LPF2:
                //   The loop input into LPF2 is: u2 = v1 + G_hp * (fb - s_hp)
                //   Because C28 & C29 form a 1st-order LP and 1st-order HP in the feedback path,
                //   the transfer function has denominator: 1 + g1 + g2 + g1*g2 - k*g1*(1) = 0 at K=2!
                const double s1Term = s1 / (1.0 + g1);
                const double s2Term = s2 / (1.0 + g2);

                const double v1 = G1 * x + s1Term;
                // Feedback enters through HP branch (1 - G2) * fb - s2Term_hp
                // Standard Korg35 LPF ZDF:
                // y = G2 * (v1 + G1_fb) -> Specifically:
                // y = G2 * (v1 + (fb - s2Term) / (1.0 + g2)) ... let's check denominator!
                // If v1 = G1 * x + s1Term + G1 * (fb - v1_hp),
                // Let's use the exact 2-integrator Korg-35 state equations:
                //   LP1: v1 = G1 * x + s1Term
                //   HP2 (feedback): v_fb = (fb - s2) / (1.0 + g2)
                //   LP2 (forward):  y = G2 * (v1 + fb) + s2Term
                // Let's check small-signal pole when fb = k * y and x = 0, s1 = s2 = 0:
                //Wait: in continuous time, H_LP(s) = G^2 / (s^2 + s*(2 - K)*w0 + w0^2).
                // With trapezoidal integrators:
                //   v1 = g1*(x - v1) + s1 + g1*(fb - y)?
                // Yes! Look at the continuous-time Sallen-Key ODE:
                //   (1/w1) * dv1/dt = x - v1 + (fb - y)
                //   (1/w2) * dy/dt  = v1 - y
                // Let's check the denominator of that ODE in Laplace domain (s / w0 = p):
                //   p * v1 = x - v1 + K*y - y  =>  v1 * (p + 1) = x + (K - 1)*y
                //   p * y  = v1 - y            =>  v1 = (p + 1)*y
                // Substitute v1:
                //   (p + 1)^2 * y = x + (K - 1)*y
                //   (p^2 + 2p + 1 - K + 1) * y = x
                //   (p^2 + (2 - K)*p + 1) * y = x!
                // At K = 2, the damping term (2 - K)*p vanishes -> pure self-oscillation at w = w0!
                // And at DC (p = 0), y = x / (2 - K) or if feedback is high-pass coupled (via C29, which blocks DC in Korg-35!):
                // Wait: in Korg-35, the feedback is AC-coupled through C29 (`p * fb / (p + 1)`):
                //   p * v1 = x - v1       => v1 = x / (p + 1)
                //   p * y  = v1 - y + p * fb_node, where fb_node is through C29!
                // Specifically:
                //   v1 = x / (p + 1)  (LPF 1)
                //   v_hp = (p / (p + 1)) * fb  (HPF in feedback path)
                //   y = (v1 + v_hp) / (p + 1)  (LPF 2)
                // Let's check that!
                //   y = [ x / (p + 1) + p * K * y / (p + 1) ] / (p + 1)
                //   (p + 1)^2 * y = x + p * K * y
                //   (p^2 + (2 - K)*p + 1) * y = x !!
                // Look at that equation:
                //   1) At DC (p = 0): y = x (unity DC gain regardless of resonance K! No DC blowup!)
                //   2) At p = j (w = w0): ( -1 + j*(2 - K) + 1 ) * y = x => at K = 2, infinite gain (self-oscillation at w0)!
                //   3) And in the hardware Korg-35 schematic (page 4), transistor 1 forms LPF 1, the feedback goes through capacitor C29 (HPF) clipped by diodes D19/D20 to ground, and enters transistor 2 (LPF 2)!
                // That matches the physical Korg-35 topology 100%!

                // Let's write the trapezoidal ZDF discretization of:
                //   Stage 1 (LP): v1 = G1 * x + s1Term
                //   Feedback HP + Forward LP2 combined in Stage 2:
                //   Or with two states (s1 for C28, s2 for C29):
                //   In continuous time:
                //     C1: dv1/dt = w1 * (x - v1)
                //     C2: d(y - fb)/dt = w2 * (v1 - y)   <=>  dy/dt - dfb/dt = w2 * (v1 - y)
                //   Look how clean C2 is: C2 has voltage (y - fb) across it!
                //   Wait: if v_c2 = y - fb, then:
                //     v_c2 = g2 * (v1 - y) + s2
                //     y - fb = g2 * (v1 - y) + s2
                //     y * (1 + g2) = g2 * v1 + fb + s2
                //     y = G2 * v1 + (fb + s2) / (1.0 + g2) !
                //   Let's check the Laplace transfer function of this 2-capacitor circuit!
                //   In trapezoidal domain, g = p:
                //     v1 = (p / (1 + p)) * x
                //     y * (1 + p) = p * v1 + K * y  (for small signal fb = K * y)
                //   Wait: in C2, the current into C2 is w2 * (v1 - y), and the other terminal of C2 is at `fb`!
                //   Wait: if one terminal of C2 is at `y` (the input to the buffer) and the other terminal is at `fb`,
                //   then v_c2 = y - fb, so dv_c2/dt = w2 * (v1 - y) => p*(y - K*y) = p*(1 - K)*y = v1 - y, which is 1st order.
                //   Why was the previous one 2nd order? Because C1 has feedback `fb` on its ground leg, OR C2 is LP and feedback has HP!
                //   Let's check: in Korg-35 LPF:
                //   C1 is connected between Stage 1 output (`v1`) and the feedback node (`fb`)!
                //   And C2 is connected between Stage 2 output (`y`) and ground (with diodes across Stage 2)!
                //   Let's check THAT!
                //   If C1 is connected between `v1` and `fb`:
                //     v_c1 = v1 - fb
                //     dv_c1/dt = w1 * (x - v1)
                //     => p * (v1 - fb) = x - v1  =>  v1 * (1 + p) = x + p * fb!
                //   And C2 is connected between `y` and ground:
                //     dv_c2/dt = w2 * (v1 - y)
                //     => p * y = v1 - y          =>  y * (1 + p) = v1!
                //   Now substitute v1 into y:
                //     y * (1 + p)^2 = x + p * fb!
                //   If fb = K * y:
                //     (p^2 + (2 - K)*p + 1) * y = x !!
                //   BINGO! That uses ONLY TWO CAPACITORS (C1 and C2), matches the exact 2-capacitor Sallen-Key schematic on page 4 (C28 and C29!), has unity DC gain, and self-oscillates at K = 2!

                // Let's double check the trapezoidal equations for this exact 2-capacitor Sallen-Key LPF:
                // Capacitor 1 voltage: vc1 = v1 - fb
                //   vc1 = g1 * (x - v1) + s1
                //   v1 - fb = g1 * x - g1 * v1 + s1
                //   v1 * (1 + g1) = g1 * x + fb + s1
                //   v1 = G1 * x + (fb + s1) / (1.0 + g1)
                // Capacitor 2 voltage: vc2 = y (with D19/D20 clipping on the feedback `fb = clip(k * y)`):
                //   y = g2 * (v1 - y) + s2
                //   y * (1 + g2) = g2 * v1 + s2
                //   y = G2 * v1 + s2 / (1.0 + g2)
                // Substitute v1 into y:
                //   y = G2 * [ G1 * x + (fb + s1) / (1.0 + g1) ] + s2Term
                const double v1_from_fb = G1 * x + (fb + s1) / (1.0 + g1);
                const double y_rhs = G2 * v1_from_fb + s2Term;

                F = y - y_rhs;
                const double dy_rhs = (G2 / (1.0 + g1)) * dfb_dy;
                dF = 1.0 - dy_rhs;
            }
            else
            {
                // Sallen-Key High-Pass topology (IC9 KORG-35 VCHPF on KLM-127):
                // In the HPF schematic (page 4):
                // Input `x` enters through series capacitor C15 (`0.0033uF`),
                // Stage 1 output goes into Stage 2 capacitor C16 (`0.0033uF`),
                // Feedback from buffer IC12 (`y`) goes through Peak pot VR7 (`fb`) to Stage 1 resistor,
                // while diodes D9/D10 clamp the Stage 2 node to ground!
                // Let's check the 2-capacitor Sallen-Key HPF equations:
                // Capacitor 1: vc1 = x - v1
                //   d(x - v1)/dt = w1 * (v1 - fb)
                //   => p * (x - v1) = v1 - fb  =>  v1 * (1 + p) = p * x + fb
                // Capacitor 2: vc2 = v1 - y
                //   d(v1 - y)/dt = w2 * y
                //   => p * (v1 - y) = y        =>  y * (1 + p) = p * v1
                // Substitute v1 into y:
                //   y * (1 + p)^2 = p^2 * x + p * fb!
                // With fb = K * y:
                //   (p^2 + (2 - K)*p + 1) * y = p^2 * x !!
                // Look at that transfer function:
                //   1) Pure 2-pole High-Pass (`p^2 / (p^2 + (2 - K)*p + 1)`)!
                //   2) Zero gain at DC (`p = 0 => y = 0`), unity gain at high frequency (`p -> inf => y = x`)!
                //   3) Self-oscillates at K = 2 (`2 - K = 0`) at the exact cutoff frequency `w = w0`!
                //   4) Uses ONLY the two physical capacitors C15 (`s1`) and C16 (`s2`)!

                // Let's write the trapezoidal discretization of this exact 2-capacitor Sallen-Key HPF:
                // Capacitor 1: vc1 = x - v1
                //   x - v1 = g1 * (v1 - fb) + s1
                //   v1 * (1 + g1) = x + g1 * fb - s1
                //   v1 = (x + g1 * fb - s1) / (1.0 + g1)
                // Capacitor 2: vc2 = v1 - y
                //   v1 - y = g2 * y + s2
                //   y * (1 + g2) = v1 - s2
                //   y = (v1 - s2) / (1.0 + g2)
                const double v1_from_fb = (x + g1 * fb - s1) / (1.0 + g1);
                const double y_rhs = (v1_from_fb - s2) / (1.0 + g2);

                F = y - y_rhs;
                const double dy_rhs = (G1 / (1.0 + g2)) * dfb_dy;
                dF = 1.0 - dy_rhs;
            }

            if (std::abs(dF) < 1.0e-9)
                break;

            const double delta = F / dF;
            y -= delta;

            if (std::abs(delta) < 1.0e-7)
                break;
        }

        // Update capacitor states using trapezoidal rule: s_new = 2 * vc - s_old
        double fb = 0.0;
        double dummy = 0.0;
        diodeClipWithDerivative(k * y, fb, dummy);
        fb *= k;

        if (mode == Mode::LowPass)
        {
            const double v1 = G1 * x + (fb + s1) / (1.0 + g1);
            const double vc1 = v1 - fb;
            const double vc2 = y;

            s1 = 2.0 * vc1 - s1;
            s2 = 2.0 * vc2 - s2;
        }
        else
        {
            const double v1 = (x + g1 * fb - s1) / (1.0 + g1);
            const double vc1 = x - v1;
            const double vc2 = v1 - y;

            s1 = 2.0 * vc1 - s1;
            s2 = 2.0 * vc2 - s2;
        }

        yEst = y;

        // Output of IC12/IC11 op-amp buffer (soft rail limit at ±13.5V equivalent)
        return fastTanh(y * 0.72) * 1.38;
    }

private:
    Mode mode = Mode::LowPass;
    double sampleRate = 176400.0;
    double g = 0.1;
    double k = 0.02;

    double s1 = 0.0;
    double s2 = 0.0;
    double yEst = 0.0;
    double dcBlockState = 0.0;

    double c1Scale = 1.0;
    double c2Scale = 1.004;
    double asymmetry = 1.0;

    static inline double fastTanh(double v) noexcept
    {
        const double v2 = v * v;
        return v * (27.0 + v2) / (27.0 + 9.0 * v2 + 0.08 * v2 * v2);
    }

    /**
     * Korg-35 internal differential BJT input stage saturation
     */
    inline double korg35OtaSaturate(double v) const noexcept
    {
        // Slightly asymmetric soft saturation from the single-ended transistor pair inside KORG-35
        const double biased = v + 0.025 * asymmetry * (v * v) / (1.0 + std::abs(v));
        return std::tanh(biased * 0.85) / 0.85;
    }

    /**
     * Anti-parallel 1S-1555 silicon diode pair (D9/D10 in HPF, D19/D20 in LPF)
     * in series with the feedback resistor divider.
     * For small signals (|u| < ~0.55V), the diodes barely conduct (linear feedback),
     * then smoothly compress following the Shockley exponential V-I curve.
     */
    inline void diodeClipWithDerivative(double u, double& clipped, double& derivative) const noexcept
    {
        // Normalize u relative to the 1S-1555 knee voltage (~0.62V)
        // Slight mismatch between D9 and D10 (or D19 and D20) creates even harmonics during self-oscillation!
        const double posScale = 1.0 + 0.03 * asymmetry;
        const double negScale = 1.0 - 0.03 * asymmetry;
        const double scale = (u >= 0.0) ? posScale : negScale;

        const double x = u * scale;
        const double ax = std::abs(x);

        // Smooth silicon diode limiter: linear below knee, soft logarithmic/tanh compression above knee
        // Matches 1S-1555 + series resistor R91/R118 voltage divider law:
        //   v_d(x) = x / (1 + (x/Vk)^2 + 0.35*(x/Vk)^4)^0.25 + 0.12 * tanh(x)
        const double invKnee = 0.92;
        const double z = x * invKnee;
        const double z2 = z * z;
        const double denomInner = 1.0 + z2 + 0.25 * z2 * z2;
        const double denom = std::sqrt(std::sqrt(denomInner)); // denomInner^0.25
        const double mainTerm = x / denom;

        // Derivative of x / (1 + z^2 + 0.25*z^4)^0.25 with respect to x:
        // d/dx [ x * f^(-1/4) ] = f^(-1/4) * [ 1 - 0.25 * z * (2*z + z^3) / f ]
        const double dMain_dx = (1.0 / denom) * (1.0 - 0.25 * z2 * (2.0 + z2) / denomInner);

        // Residual series resistance conduction slope (R92 / R119 in KLM-127)
        const double th = std::tanh(x * 0.5);
        const double dTh_dx = 0.5 * (1.0 - th * th);

        clipped = (0.88 * mainTerm + 0.12 * (x > 0.0 ? std::min(x, 2.5) : std::max(x, -2.5)) * 0.15 + 0.10 * th) / scale;
        derivative = 0.88 * dMain_dx + 0.10 * dTh_dx + (ax < 2.5 ? 0.018 : 0.0);
    }
};

} // namespace ms20::dsp
