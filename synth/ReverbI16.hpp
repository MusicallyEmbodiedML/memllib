#pragma once

#include "maximilian.h"
#include <utility>
#include <cmath>

// Freeverb-style reverb using int16 delay lines.
// 4 parallel LP-filtered feedback combs → 2 serial Schroeder allpasses → stereo out.
// COMB_SIZE must be a power of 2 (DynamicDelayI16 bitmask requirement).
// At default COMB_SIZE=4096: ~40 KB RAM, ~75 ops/sample ≈ 1.5% CPU at 200MHz/48kHz.
template<size_t COMB_SIZE = 4096>
class ReverbI16 {
    static_assert((COMB_SIZE & (COMB_SIZE - 1)) == 0, "COMB_SIZE must be a power of 2");
    static constexpr size_t AP_SIZE    = COMB_SIZE / 4;  // 1024
    static constexpr size_t PRE_SIZE   = COMB_SIZE / 2;  // 2048 (~42ms at 48kHz)
    static constexpr size_t DECOR_SIZE = 64;

    // Comb bases as fraction of sample rate; scale range [0.5, 1.1] keeps max < COMB_SIZE
    static constexpr float kCombBases[4] = {0.0500f, 0.0561f, 0.0625f, 0.0688f};
    // Allpass fixed times in samples (not size-scaled; fit within AP_SIZE=1024)
    static constexpr float kAPTimes[2] = {605.f, 480.f};

    DynamicDelayI16<COMB_SIZE> combs_[4];
    DynamicDelayI16<AP_SIZE>   aps_[2];
    DynamicDelayI16<PRE_SIZE>  preDelay_;
    DynamicDelayI16<DECOR_SIZE> decorR_;

    float dampState_[4] = {};
    float hpfLpState_   = 0.f;

    float lfo1_ = 0.f;
    float lfo2_ = 0.5f;

    // Written by ProcessParams (Core 1 bg), read by process() ISR (Core 1 hi).
    // Same-core aligned float reads/writes are safe on Cortex-M33.
    float combTimes_[4]    = {};
    float feedbackGain_    = 0.60f;
    float dampCoeff_       = 0.50f;
    float apGain_          = 0.50f;
    float modDepth_        = 0.f;
    float modInc_          = 0.f;
    float preDelaySamples_ = 0.f;
    float hpfCoeff_        = 0.f;
    float width_           = 0.7f;
    float satDrive_        = 0.f;
    float sampleRate_      = 48000.f;

public:
    void setup(float sr) {
        sampleRate_ = sr;
        // Fast smoothing on combs so LFO modulation tracks correctly
        for (auto& c : combs_) c.setSmoothCoeff(0.99f);
        setSize(0.5f);
    }

    // 0-1 → scale 0.5-1.1; combTimes_ derived from base × scale × sr
    void setSize(float v) {
        const float scale = 0.5f + v * 0.6f;
        const float maxT  = static_cast<float>(COMB_SIZE - 2);
        for (int i = 0; i < 4; ++i)
            combTimes_[i] = fminf(kCombBases[i] * sampleRate_ * scale, maxT);
    }

    void setDecay(float v)       { feedbackGain_    = 0.30f + v * 0.67f; }
    void setDamping(float v)     { dampCoeff_       = v * 0.92f; }
    void setDiffusion(float v)   { apGain_          = 0.30f + v * 0.45f; }
    void setModDepth(float v)    { modDepth_        = v * 20.f; }
    void setModRate(float v)     { modInc_          = (0.1f + v * 4.9f) / sampleRate_; }
    void setPreDelay(float v)    { preDelaySamples_ = v * static_cast<float>(PRE_SIZE - 1); }
    void setLowCut(float v)      { hpfCoeff_        = v * 0.05f; }
    void setStereoWidth(float v) { width_           = v; }
    void setSaturation(float v)  { satDrive_        = v * 4.f; }

    // Returns wet-only stereo pair; caller applies wet/dry crossfade.
    std::pair<float, float> __force_inline process(float in) {
        // Input HPF (1-pole: y_lp += coeff*(x - y_lp); y_hp = x - y_lp).
        // coeff IS the cutoff: 0 => y_lp frozen => y_hp = x (no cut, full signal).
        // hpfCoeff_ in [0, 0.05] keeps the low cut gentle (~0..400 Hz at 48k).
        hpfLpState_ += hpfCoeff_ * (in - hpfLpState_);
        float sig = in - hpfLpState_;

        // Pre-delay
        if (preDelaySamples_ >= 1.f) {
            const float pd = preDelay_.read(preDelaySamples_);
            preDelay_.write(sig);
            sig = pd;
        }

        // Dual triangle LFOs (antiphase: lfo2 offset by 0.5 period, runs at 1.3× rate)
        lfo1_ += modInc_;
        if (lfo1_ >= 1.f) lfo1_ -= 1.f;
        lfo2_ += modInc_ * 1.3f;
        if (lfo2_ >= 1.f) lfo2_ -= 1.f;
        const float tri1 = lfo1_ < 0.5f ? 4.f * lfo1_ - 1.f : 3.f - 4.f * lfo1_;
        const float tri2 = lfo2_ < 0.5f ? 4.f * lfo2_ - 1.f : 3.f - 4.f * lfo2_;

        // 4 parallel LP-comb filters
        float combSum = 0.f;
        for (int i = 0; i < 4; ++i) {
            const float lfo = (i < 2) ? tri1 : tri2;
            const float y   = combs_[i].read(combTimes_[i] + lfo * modDepth_);
            dampState_[i]   = dampState_[i] * dampCoeff_ + y * (1.f - dampCoeff_);
            combs_[i].write(sig + dampState_[i] * feedbackGain_);
            combSum += dampState_[i];
        }
        combSum *= 0.25f;

        // Optional tail saturation (normalised fasttanh)
        if (satDrive_ > 0.f) {
            const float xd = combSum * satDrive_;
            const float x2 = xd * xd;
            combSum = (xd * (27.f + x2) / (27.f + 9.f * x2)) / satDrive_;
        }

        // 2 serial Schroeder allpasses
        for (int i = 0; i < 2; ++i) {
            const float delayed = aps_[i].read(kAPTimes[i]);
            const float v = combSum - delayed * apGain_;
            aps_[i].write(v);
            combSum = v * apGain_ + delayed;
        }

        // Stereo decorrelation on R (fixed 23-sample delay)
        const float L = combSum;
        const float R = decorR_.read(23.f);
        decorR_.write(combSum);

        // Width (mid-side)
        const float mid  = (L + R) * 0.5f;
        const float side = (L - R) * 0.5f;
        return { mid + side * width_, mid - side * width_ };
    }
};
