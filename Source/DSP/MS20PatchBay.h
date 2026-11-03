#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ms20::dsp
{

enum class PatchSource : int
{
    MG_TriOut = 0,       // J3  MG Triangle / Saw Out
    MG_RectOut,          // J7  MG Rectangle / Pulse Out
    EG1_Out,             // J12 EG-1 Normal Out
    EG1_RevOut,          // J11 EG-1 Reverse Out
    EG2_RevOut,          // J23 EG-2 Reverse Out
    SH_Out,              // J8  Sample & Hold Out
    MVCA_Out,            // J16 Modulation VCA Out
    PinkNoise_Out,       // J19 Pink Noise Out
    WhiteNoise_Out,      // J25 White Noise Out
    Wheel_Out,           // J28 Control Wheel Out
    MomentarySw_Out,     // J31 Momentary Switch Out
    KbdCv_Out,           // J33 Keyboard CV Out
    KbdTrig_Out,         // J32 Keyboard Trigger Out
    Signal_Out,          // J35 Main Signal Out
    Phones_Out,          // J34 Phones Out (High Gain)
    ESP_AmpOut,          // J9  ESP Preamp Out
    ESP_FilterOut,       // J17 ESP Bandpass Filter Out
    ESP_FvCvOut,         // J18 ESP F->V Pitch CV Out
    ESP_EnvOut,          // J29 ESP Envelope Follower Out
    ESP_TrigOut,         // J30 ESP Trigger Out
    Count
};

enum class PatchDest : int
{
    TotalExt_In = 0,     // J4  TOTAL Mod In (breaks normalled MG Tri to VCO/HPF/LPF)
    FreqExt_In,          // J6  FREQ Mod In (breaks normalled EG-1 to VCO)
    ExtSig_In,           // J13 EXT SIG IN (audio sum into VCO Mixer -> HPF)
    HpfCutoff_In,        // J14 HPF Cutoff Mod In (breaks normalled EG-2 to HPF)
    LpfCutoff_In,        // J21 LPF Cutoff Mod In (breaks normalled EG-2 to LPF)
    InitialGain_In,      // J22 VCA Initial Gain In (adds to EG-2 in main VCA)
    Vco12Cv_In,          // J26 VCO 1+2 CV In (breaks normalled KBD CV)
    Vco2Cv_In,           // J24 VCO 2 CV In (breaks normalled VCO 1+2 CV to VCO 2)
    Eg1Trig_In,          // J20 EG-1 Trig In (breaks normalled KBD Trig to EG-1)
    Eg12Trig_In,         // J27 EG 1+2 External Trig In
    SH_In,               // J2  Sample & Hold Signal In
    SH_ClockIn,          // J5  Sample & Hold Clock In
    MVCA_In,             // J10 Modulation VCA Signal In
    MVCA_ControlIn,      // J15 Modulation VCA Control CV In
    ESP_SigIn,           // J1  ESP Signal In (overrides host audio input when patched)
    Count
};

struct PatchCable
{
    PatchSource source = PatchSource::MG_TriOut;
    PatchDest dest = PatchDest::TotalExt_In;
    uint32_t colorArgb = 0xFFE53935;
};

/**
 * Lock-free Patch Bay matrix for the 35-jack KLM-128C panel.
 * Because each physical input jack on an MS-20 holds at most one 1/4" plug,
 * we can store the connected PatchSource (or -1 if unpatched) per PatchDest in an atomic array!
 */
class MS20PatchBay
{
public:
    static constexpr int numSources = static_cast<int>(PatchSource::Count);
    static constexpr int numDests   = static_cast<int>(PatchDest::Count);

    MS20PatchBay() noexcept
    {
        clearAll();
    }

    void clearAll() noexcept
    {
        for (auto& d : destToSource)
            d.store(-1, std::memory_order_relaxed);
    }

    void connect(PatchSource src, PatchDest dst) noexcept
    {
        const int dIdx = static_cast<int>(dst);
        const int sIdx = static_cast<int>(src);
        if (dIdx >= 0 && dIdx < numDests && sIdx >= 0 && sIdx < numSources)
            destToSource[static_cast<size_t>(dIdx)].store(sIdx, std::memory_order_relaxed);
    }

    void disconnectDest(PatchDest dst) noexcept
    {
        const int dIdx = static_cast<int>(dst);
        if (dIdx >= 0 && dIdx < numDests)
            destToSource[static_cast<size_t>(dIdx)].store(-1, std::memory_order_relaxed);
    }

    void disconnectSource(PatchSource src) noexcept
    {
        const int sIdx = static_cast<int>(src);
        for (auto& d : destToSource)
        {
            if (d.load(std::memory_order_relaxed) == sIdx)
                d.store(-1, std::memory_order_relaxed);
        }
    }

    inline bool isConnected(PatchDest dst) const noexcept
    {
        return destToSource[static_cast<size_t>(dst)].load(std::memory_order_relaxed) >= 0;
    }

    inline int getConnectedSourceIndex(PatchDest dst) const noexcept
    {
        return destToSource[static_cast<size_t>(dst)].load(std::memory_order_relaxed);
    }

    /**
     * Read patched signal at destination, or fallback to normalled signal if unpatched.
     */
    inline double read(PatchDest dst,
                       const std::array<double, numSources>& sourceSignals,
                       double normalledValue = 0.0) const noexcept
    {
        const int sIdx = destToSource[static_cast<size_t>(dst)].load(std::memory_order_relaxed);
        if (sIdx >= 0 && sIdx < numSources)
            return sourceSignals[static_cast<size_t>(sIdx)];
        return normalledValue;
    }

    std::vector<PatchCable> getActiveCables() const
    {
        static constexpr std::array<uint32_t, 6> cablePalette = {
            0xFFF2C500, // Korg Yellow (classic MS-20 patch cord)
            0xFFD93829, // Korg Red
            0xFF2874A6, // Korg Blue
            0xFFE67E22, // Korg Orange
            0xFF27AE60, // Korg Green
            0xFFECF0F1  // Korg White
        };

        std::vector<PatchCable> cables;
        int cableIdx = 0;
        for (int d = 0; d < numDests; ++d)
        {
            const int s = destToSource[static_cast<size_t>(d)].load(std::memory_order_relaxed);
            if (s >= 0 && s < numSources)
            {
                const uint32_t color = cablePalette[static_cast<size_t>(cableIdx % cablePalette.size())];
                cables.push_back({ static_cast<PatchSource>(s), static_cast<PatchDest>(d), color });
                ++cableIdx;
            }
        }
        return cables;
    }

    static const char* getSourceName(PatchSource src) noexcept
    {
        switch (src)
        {
            case PatchSource::MG_TriOut:       return "MG TRI OUT";
            case PatchSource::MG_RectOut:      return "MG RECT OUT";
            case PatchSource::EG1_Out:         return "EG 1 OUT";
            case PatchSource::EG1_RevOut:      return "EG 1 REV OUT";
            case PatchSource::EG2_RevOut:      return "EG 2 REV OUT";
            case PatchSource::SH_Out:          return "S/H OUT";
            case PatchSource::MVCA_Out:        return "VCA OUT";
            case PatchSource::PinkNoise_Out:   return "PINK NOISE";
            case PatchSource::WhiteNoise_Out:  return "WHITE NOISE";
            case PatchSource::Wheel_Out:       return "WHEEL OUT";
            case PatchSource::MomentarySw_Out: return "MOMENTARY SW";
            case PatchSource::KbdCv_Out:       return "KBD CV OUT";
            case PatchSource::KbdTrig_Out:     return "KBD TRIG OUT";
            case PatchSource::Signal_Out:      return "SIGNAL OUT";
            case PatchSource::Phones_Out:      return "PHONES OUT";
            case PatchSource::ESP_AmpOut:      return "ESP AMP OUT";
            case PatchSource::ESP_FilterOut:   return "ESP BPF OUT";
            case PatchSource::ESP_FvCvOut:     return "ESP F-V CV";
            case PatchSource::ESP_EnvOut:      return "ESP ENV OUT";
            case PatchSource::ESP_TrigOut:     return "ESP TRIG OUT";
            case PatchSource::Count:           break;
        }
        return "";
    }

    static const char* getDestName(PatchDest dst) noexcept
    {
        switch (dst)
        {
            case PatchDest::TotalExt_In:    return "TOTAL";
            case PatchDest::FreqExt_In:     return "FREQ";
            case PatchDest::ExtSig_In:      return "EXT SIG IN";
            case PatchDest::HpfCutoff_In:   return "HPF Fc MOD";
            case PatchDest::LpfCutoff_In:   return "LPF Fc MOD";
            case PatchDest::InitialGain_In: return "INITIAL GAIN";
            case PatchDest::Vco12Cv_In:     return "VCO 1+2 CV IN";
            case PatchDest::Vco2Cv_In:      return "VCO 2 CV IN";
            case PatchDest::Eg1Trig_In:     return "EG 1 TRIG IN";
            case PatchDest::Eg12Trig_In:    return "EXT TRIG IN";
            case PatchDest::SH_In:          return "S/H IN";
            case PatchDest::SH_ClockIn:     return "S/H CLOCK";
            case PatchDest::MVCA_In:        return "VCA IN";
            case PatchDest::MVCA_ControlIn: return "CONTROL IN";
            case PatchDest::ESP_SigIn:      return "ESP SIG IN";
            case PatchDest::Count:          break;
        }
        return "";
    }

private:
    std::array<std::atomic<int>, numDests> destToSource;
};

} // namespace ms20::dsp
