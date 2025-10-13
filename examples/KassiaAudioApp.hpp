#ifndef __KASSIA_AUDIO_APP_HPP__
#define __KASSIA_AUDIO_APP_HPP__

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../audio/AudioAppBase.hpp"
#include "../audio/AudioDriver.hpp"
#include "../synth/maximilian.h"
#include "../../daisysp/Effects/pitchshifter.h"
#include "../synth/OnePoleSmoother.hpp"
#include "../utils/sharedMem.hpp"

//todo: convert to new templated base
#if 0

class KassiaAudioApp : public AudioAppBase
{
public:
    static constexpr size_t kN_Params = 14;

    KassiaAudioApp() : AudioAppBase(), smoother(150.f, kSampleRate),
        neuralNetOutputs(kN_Params, 0),
        smoothParams(kN_Params, 0)
     {}

    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input = false,
            .line_level = 7,
            .mic_gain_dB = 0,
            .output_volume = 0.95f
        };
    }


    stereosample_t Process(const stereosample_t x) override;

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override;

    void ProcessParams(const std::vector<float>& params) override;

protected:

    maxiDelayline<5000> dl1;
    maxiDelayline<15100> dl2;
    maxiDelayline<1201> dl3;

    maxiReverbFilters<300> allp1;
    maxiReverbFilters<1000> allp2;
    maxiReverbFilters<300> comb1;
    maxiReverbFilters<1000> comb2;

    std::vector<float> neuralNetOutputs, smoothParams;


    float frame=0;
    float dl1mix = 0.0f;
    float dl2mix = 0.0f;
    float dl3mix = 0.0f;
    float allp1fb=0.5f;
    float allp2fb=0.5f;

    // //listening
    // maxiBiquad bpf1;
    // maxiBiquad bpf2;
    // maxiBiquad bpf3;
    // maxiBiquad bpf4;

    // maxiEnvelopeFollowerF bpfEnv1;
    // maxiEnvelopeFollowerF bpfEnv2;
    // maxiEnvelopeFollowerF bpfEnv3;
    // maxiEnvelopeFollowerF bpfEnv4;

    maxiDCBlocker dcb;

    OnePoleSmoother<kN_Params> smoother;
    daisysp::PitchShifter pitchshifter_;
    float pitchshifter_mix_{0.5f};
    float wetdry_mix_{0.5f};
};

#endif

#endif  // KASSIA_AUDIO_APP_HPP__
