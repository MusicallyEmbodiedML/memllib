#pragma once

#include <cmath>
#include <cstddef>
#include "maximilian.h"
#include "../audio/AudioDriver.hpp"

template<size_t BUFSIZE = 16384, size_t NGRAINS = 4>
class GrainDelayI16 {
    static_assert((BUFSIZE & (BUFSIZE - 1)) == 0, "BUFSIZE must be a power of 2");
    static constexpr size_t kEnvSize = 512;
    static constexpr float kBufSizeF = static_cast<float>(BUFSIZE);
    static constexpr float kNGrainGain = 2.0f / static_cast<float>(NGRAINS);

public:
    void setup(float sample_rate) {
        sample_rate_ = sample_rate;
        for (size_t i = 0; i < kEnvSize; ++i) {
            float phase = static_cast<float>(i) / static_cast<float>(kEnvSize);
            env_[i] = 0.5f * (1.f - cosf(2.f * M_PI * phase));
        }
        for (size_t g = 0; g < NGRAINS; ++g) {
            phase_[g]    = static_cast<float>(g) / static_cast<float>(NGRAINS);
            read_pos_[g] = 0.f;
        }
        updateGrainPitches();
    }

    float __force_inline process(float input) {
        if (!frozen_) buf_.write(input + out_ * feedback_ + tap_out_ * tap_feedback_);

        float sum = 0.f;
        const float write_idx = static_cast<float>(buf_.getWriteIndex());

        float tap_pos = write_idx - start_time_;
        if (tap_pos < 0.f) tap_pos += kBufSizeF;
        tap_out_ = buf_.readAbsolute(tap_pos);

        for (size_t g = 0; g < NGRAINS; ++g) {
            phase_[g] += phase_inc_;
            if (phase_[g] >= 1.0f) {
                phase_[g] -= 1.0f;
                read_pos_[g] = write_idx - start_time_;
                if (read_pos_[g] < 0.f) read_pos_[g] += kBufSizeF;
            }
            const size_t env_idx = static_cast<size_t>(phase_[g] * kEnvSize) & (kEnvSize - 1);
            sum += buf_.readAbsolute(read_pos_[g]) * env_[env_idx];
            read_pos_[g] += grain_pitch_[g];
            if (read_pos_[g] >= kBufSizeF) read_pos_[g] -= kBufSizeF;
            if (read_pos_[g] < 0.f)        read_pos_[g] += kBufSizeF;
        }
        out_ = sum * kNGrainGain;  // grains only — keeps tap out of the grain feedback path
        return out_ + tap_out_ * tap_level_;
    }

    stereosample_t __force_inline processStereo(float input) {
        if (!frozen_) buf_.write(input + out_ * feedback_ + tap_out_ * tap_feedback_);

        float sumL = 0.f, sumR = 0.f;
        const float write_idx = static_cast<float>(buf_.getWriteIndex());

        float tap_pos = write_idx - start_time_;
        if (tap_pos < 0.f) tap_pos += kBufSizeF;
        tap_out_ = buf_.readAbsolute(tap_pos);

        for (size_t g = 0; g < NGRAINS; ++g) {
            phase_[g] += phase_inc_;
            if (phase_[g] >= 1.0f) {
                phase_[g] -= 1.0f;
                read_pos_[g] = write_idx - start_time_;
                if (read_pos_[g] < 0.f) read_pos_[g] += kBufSizeF;
            }
            const size_t env_idx = static_cast<size_t>(phase_[g] * kEnvSize) & (kEnvSize - 1);
            const float grainOut = buf_.readAbsolute(read_pos_[g]) * env_[env_idx];
            if (g & 1) sumR += grainOut;
            else       sumL += grainOut;
            read_pos_[g] += grain_pitch_[g];
            if (read_pos_[g] >= kBufSizeF) read_pos_[g] -= kBufSizeF;
            if (read_pos_[g] < 0.f)        read_pos_[g] += kBufSizeF;
        }
        out_ = (sumL + sumR) * kNGrainGain;
        const float tapContrib = tap_out_ * tap_level_;
        return { sumL * kNGrainGain + tapContrib,
                 sumR * kNGrainGain + tapContrib };
    }

    void setGrainLengthSamples(float samples) { phase_inc_ = 1.0f / samples; }
    void setGrainLengthMs(float ms)  { setGrainLengthSamples(ms * 0.001f * sample_rate_); }
    void setStartTimeSamples(float s) { start_time_ = s; }
    void setStartTimeMs(float ms)    { setStartTimeSamples(ms * 0.001f * sample_rate_); }
    void setFeedback(float fb)       { feedback_ = fb; }
    void setPitch(float ratio)       { pitch_ = ratio; updateGrainPitches(); }
    void setPitchSpread(float spread){ pitch_spread_ = spread; updateGrainPitches(); }
    void setTapLevel(float level)    { tap_level_ = level; }
    void setTapFeedback(float fb)    { tap_feedback_ = fb; }
    void setFreeze(bool freeze)      { frozen_ = freeze; }

private:
    DynamicDelayI16<BUFSIZE> buf_;
    float env_[kEnvSize] = {};
    float phase_[NGRAINS]    = {};
    float read_pos_[NGRAINS] = {};
    float phase_inc_  = 1.0f / 4800.f;  // default 100ms at 48kHz
    float start_time_ = 8000.f;          // default ~167ms
    float feedback_   = 0.3f;
    float pitch_      = 1.0f;
    float pitch_spread_ = 0.f;
    float grain_pitch_[NGRAINS] = {};
    float tap_level_   = 0.f;
    float tap_feedback_= 0.f;
    float tap_out_     = 0.f;
    float out_        = 0.f;
    bool  frozen_     = false;
    float sample_rate_= 48000.f;

    void updateGrainPitches() {
        for (size_t g = 0; g < NGRAINS; ++g) {
            float t = NGRAINS > 1
                ? static_cast<float>(g) / static_cast<float>(NGRAINS - 1)
                : 0.5f;
            grain_pitch_[g] = pitch_ + pitch_spread_ * (t - 0.5f);
        }
    }
};
