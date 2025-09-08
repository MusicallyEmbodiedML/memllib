#ifndef __XIASRIAUDIOAPP_HPP__
#define __XIASRIAUDIOAPP_HPP__

#include "../audio/AudioAppBase.hpp"
#include "../audio/AudioDriver.hpp"
#include "../../../sharedMem.hpp"
#include "../PicoDefs.hpp"
#include "../synth/OnePoleSmoother.hpp"
#include "../synth/maximilian.h"


class PAFSynthApp : public AudioAppBase
{
public:
    static constexpr size_t kN_Params = 12;

    PAFSynthApp() : AudioAppBase(), smoother(150.f, kSampleRate),
        neuralNetOutputs(kN_Params, 0),
        smoothParams(kN_Params, 0)
     {}




    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float mix = x.L + x.R;
        float bpf1Val = bpf1.play(mix) * 100.f;
        bpf1Val = bpfEnv1.play(bpf1Val);
        WRITE_VOLATILE(sharedMem::f0, bpf1Val);

        float bpf2Val = bpf2.play(mix) * 100.f;
        bpf2Val = bpfEnv2.play(bpf2Val);
        WRITE_VOLATILE(sharedMem::f1, bpf2Val);

        float bpf3Val = bpf3.play(mix) * 100.f;
        bpf3Val = bpfEnv3.play(bpf3Val);
        WRITE_VOLATILE(sharedMem::f2, bpf3Val);

        float bpf4Val = bpf4.play(mix) * 100.f;
        bpf4Val = bpfEnv4.play(bpf4Val);
        WRITE_VOLATILE(sharedMem::f3, bpf4Val);

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());


        dl1mix = smoothParams[0] * smoothParams[0] * 0.4f;
        dl2mix = smoothParams[1] * smoothParams[1] * 0.4f;
        dl3mix = smoothParams[2] * smoothParams[2] * 0.8f;
        allp1fb = smoothParams[4] * 0.99f;
        allp2fb = smoothParams[5] * 0.99f;
        float comb1fb = (smoothParams[6] * 0.95f);
        float comb2fb = (smoothParams[7] * 0.95f);

        float dl1fb = (smoothParams[8] * 0.95f);
        float dl2fb = (smoothParams[9] * 0.95f);
        float dl3fb = (smoothParams[10] * 0.95f);


        float y = dcb.play(mix, 0.99f) * 3.f;
        float y1 = allp1.allpass(y, 30, allp1fb);
        y1 = comb1.combfb(y1, 127, comb1fb);

        float y2 = allp2.allpass(y, 500, allp2fb);
        y2 = comb2.combfb(y2, 808, comb2fb);

        y = y1 + y2;
        float d1 = (dl1.play(y, 3500, dl1fb) * dl1mix);
        float d2 = (dl2.play(y, 9000, dl2fb) * dl2mix);
        float d3 = (dl3.play(y, 1199, dl3fb) * dl3mix);


        y = y + d1 + d2 + d3;

        y = tanhf(y);

        stereosample_t ret { y, y };

        frame++;



        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        bpf1.set(maxiBiquad::filterTypes::BANDPASS, 100.f, 5.f, 0.f);
        bpf2.set(maxiBiquad::filterTypes::BANDPASS, 500.f, 5.f, 0.f);
        bpf3.set(maxiBiquad::filterTypes::BANDPASS, 1000.f, 5.f, 0.f);
        bpf4.set(maxiBiquad::filterTypes::BANDPASS, 4000.f, 5.f, 0.f);
    }

    void ProcessParams(const std::vector<float>& params) override
    {
        neuralNetOutputs = params;

    }

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

    //listening
    maxiBiquad bpf1;
    maxiBiquad bpf2;
    maxiBiquad bpf3;
    maxiBiquad bpf4;

    maxiEnvelopeFollowerF bpfEnv1;
    maxiEnvelopeFollowerF bpfEnv2;
    maxiEnvelopeFollowerF bpfEnv3;
    maxiEnvelopeFollowerF bpfEnv4;

    maxiDCBlocker dcb;

    OnePoleSmoother<kN_Params> smoother;

};


#endif  // __XIASRIAUDIOAPP_HPP__