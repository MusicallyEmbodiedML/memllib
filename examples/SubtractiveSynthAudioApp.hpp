#ifndef __SUBTRACTIVE_SYNTH_AUDIO_APP_HPP__
#define __SUBTRACTIVE_SYNTH_AUDIO_APP_HPP__


#include <Arduino.h>
#include "../synth/maximilian.h"
#include "../audio/AudioAppBase.hpp"

class SubtractiveSynthAudioApp : public AudioAppBase
{
public:
    static constexpr size_t kN_Params = 10;

    SubtractiveSynthAudioApp() : AudioAppBase() {}

    stereosample_t Process(const stereosample_t x) override
    {
        float y = osc1.sawn(osc1freq);
        y += osc2.sawn(osc2freq);
        y += osc3.sawn(osc3freq);
        float lfo1val = (lfo1.triangle(lfo1freq) * lfo1depth);
        svf.setParams(filter1freq * (1.f + lfo1val),filter1res);
        y = svf.play(y, filterMix,1.0-filterMix,0,0);
        y *= 0.9f;
        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase::Setup(sample_rate, interface);
        // Additional setup code specific to FMSynthAudioApp
    }

    void ProcessParams(const std::vector<float>& params) override
    {
        // // Map parameters to the synth
        // synth_.mapParameters(params);
        // //DEBUG_PRINT("Params processed.");
        osc1freq = 50.f + (params[0] * 50.f);
        osc2freq = osc1freq * (1.f + (params[1] * 0.1f));
        osc3freq = osc1freq * (1.f + (params[2] * 0.5f));

        filter1freq = 80.f + (params[3] * params[3] * 5000.f);
        filter1res = (params[4] * 8.f);

        lfo1freq = 0.1f + (params[5] * params[5] * 20.f);
        lfo1depth = params[6] * 0.5f;

        filterMix = params[7];


    }

protected:
    maxiOsc osc1;
    maxiOsc osc2;
    maxiOsc osc3;
    maxiOsc lfo1;
    maxiOsc lfo2;

    float osc1freq = 100;
    float osc2freq = 101;
    float osc3freq = 102;

    float lfo1freq = 1;
    float lfo1depth = 0.1;

    maxiFilter filter1;
    maxiSVF svf;

    float filter1freq = 100;
    float filter1res = 2.f;

    float filterMix=1;

};



#endif  // __SUBTRACTIVE_SYNTH_AUDIO_APP_HPP__