#ifndef __GUITAR_AUDIO_APP_HPP__
#define __GUITAR_AUDIO_APP_HPP__


#include "../audio/AudioAppBase.hpp"
#include "../audio/AudioDriver.hpp"
#include "../synth/maximilian.h"
#include "../synth/OnePoleSmoother.hpp"
#include "../utils/Maths.hpp"


class GuitarAudioApp : public AudioAppBase
{
public:

    static constexpr size_t kNSteps = 16;

    struct AppParams {
        float steps;
        float stepsize;
        float freq[kNSteps];
        float reso[kNSteps];
        float envlength;
        //float gain[kNSteps];
    };
    static constexpr size_t kN_Params = sizeof(AppParams) / sizeof(float);

    volatile bool switch_mode;

    GuitarAudioApp() :
        switch_mode(false),
        raw_params_{0.0f},
        param_smoother_(10.0f, 48000.0f), // Default sample rate, will be updated in Setup
        osc_(),
        phasor_(0),
        step_number_(0),
        svf_(),
        AudioAppBase()
    {
        // Constructor implementation (if needed)
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        maxiSettings::setup(sample_rate, kNChannels, kBufferSize);
        AudioAppBase::Setup(sample_rate, interface);
        // Additional setup for GuitarAudioApp can be added here
        param_smoother_.setSampleRate(sample_rate);
        param_smoother_.SetTimeMs(50.0f); // Set smoothing time to 10 ms

        // Setup the line envelope: from 1.0 to 0.5 over 1000ms (1 second), one-shot
        env_.prepare(1.0f, 0.f, 100.0f, true);
        env_.triggerEnable(1); // Enable triggering
    }

    __force_inline stereosample_t ProcessInline(const stereosample_t x)
    {
        // Smooth parameters
        union {
            AppParams p;
            float arr[kN_Params];
        } params;
        param_smoother_.Process(raw_params_, params.arr);
        // Parameter scaling/assignment
        size_t n_steps = static_cast<size_t>(MapToSeries(params.p.steps, {6, 8, 10, 12, 16}));
        static constexpr float min_tempo = 70.f;
        static constexpr float max_tempo = 140;
        float tempo = LinearMap(params.p.stepsize, min_tempo, max_tempo);
        size_t subdivision_samples = CalculateSubdivisionSamples(n_steps, tempo, sample_rate_);

        // Advance phasor and check for step change
        float env_sig = 0;
        phasor_++;
        if (phasor_ >= subdivision_samples) {
            phasor_ = 0;
            step_number_ = (step_number_ + 1) % n_steps;
            env_sig = 1.f; // This will trigger the envelope
            float env_scale = LinearMap(params.p.envlength, 20.f, 300.f);
            env_.prepare(1.f, 0, env_scale, true); // Retrigger envelope
        }

        // Get parameters of current step
        float freq = ExponentialMap(params.p.freq[step_number_], 200, 800);
        // float reso = params.p.reso[step_number_];
        // float gain = params.p.gain[step_number_];
        svf_.setCutoff(freq);
        float reso = LinearMap(params.p.reso[step_number_], 0, 7.0f);
        svf_.setResonance(reso);

        float y = x.L + x.R;
        float env = env_.play(env_sig);
        if (switch_mode) {
            static constexpr float freq_adjust = 440.f/240.f;
            // Simple synthesis: sine wave at step frequency
            y = osc_.sinebuf(roundToNearestSemitone(freq) * freq_adjust) * 0.15;
            // Get the current envelope value (this runs continuously once triggered)
        } else {
            y = svf_.play(y, 0, 0, 1.f, 0);
        }
        y *= env;

        return { y, y };
    }

    void ProcessParams(const std::vector<float>& params) override
    {
        if (params.size() != kN_Params) {
            // Handle error: unexpected number of parameters
            return;
        }
        // Copy new parameters into raw_params_ for smoothing
        std::copy(params.begin(), params.end(), raw_params_);
    }

protected:

    float raw_params_[kN_Params];
    OnePoleSmoother<kN_Params> param_smoother_;
    maxiOsc osc_;
    maxiSVF svf_;
    maxiLine env_;

    size_t phasor_;
    size_t step_number_;

    static constexpr size_t CalculateSubdivisionSamples(size_t subdivision, float bpm, float sample_rate)
    {
        // Calculate seconds per beat (quarter note)
        float seconds_per_beat = 60.0f / bpm;

        // Calculate seconds per bar (4 beats in 4/4 time)
        float seconds_per_bar = seconds_per_beat * 4.0f;

        // Calculate seconds per subdivision of the bar
        float seconds_per_subdivision = seconds_per_bar / static_cast<float>(subdivision);

        // Convert to samples and round
        float samples_float = seconds_per_subdivision * sample_rate;

        return static_cast<size_t>(std::round(samples_float));
    }
};


#endif // __GUITAR_AUDIO_APP_HPP__
