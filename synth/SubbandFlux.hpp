#ifndef __SUBBANDFLUX_HPP__
#define __SUBBANDFLUX_HPP__

#include "maximilian.h"   // maxiBiquad, maxiEnvelopeFollowerF
#include <cstddef>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// SubbandFlux — FFT-free spectral-flux estimator for the audio loop.
//
// An FFT-free stand-in for spectral flux: split the signal into 3 bands (low /
// mid / high) with biquads, track each band's magnitude with an envelope
// follower, and at a control-rate hop sum the *positive* (half-wave rectified)
// changes of the energy-normalised band levels:
//
//   flux = sum_b max( norm_b[n] - norm_b[n-hop], 0 ),   norm_b = env_b / Σ env
//
// Energy-normalisation makes flux measure spectral *movement* (brightening /
// timbral change) rather than just getting louder; half-wave rectification makes
// it an onset/brightening detector. Cheap: 4 biquads + 3 followers per sample,
// differenced once per hop.
//
// Outputs (read by the DSP mapping later — updated in place, no allocation):
//   bandEnv[b]  : smoothed magnitude per band (also usable as energy/brightness)
//   bandFlux[b] : smoothed positive normalised change per band
//   fluxTotal   : summed flux (overall spectral movement)
//
// Run per sample on whatever signal you want to listen to (e.g. the output):
//   listener.setup(sample_rate);   // after maxiSettings::sampleRate is set
//   listener.process(y);           // each sample
// ---------------------------------------------------------------------------

template<size_t NBands = 3>
class SubbandFlux {
    static_assert(NBands == 3, "SubbandFlux currently builds a fixed 3-band split");
public:
    // ---- outputs ----
    float bandEnv[NBands]  = {0};
    float bandFlux[NBands] = {0};
    float fluxTotal        = 0.f;

    // Configure filters/followers. Call once after maxiSettings::sampleRate is
    // valid (e.g. from the audio app's Setup()).
    void setup(float /*sample_rate*/) {
        // 3 bands: low < ~800 Hz, mid ~800-4000 Hz, high > ~4000 Hz.
        lpLow_.set(maxiBiquad::filterTypes::LOWPASS,  800.f,  0.707f, 0.f);
        hpMid_.set(maxiBiquad::filterTypes::HIGHPASS, 800.f,  0.707f, 0.f);
        lpMid_.set(maxiBiquad::filterTypes::LOWPASS,  4000.f, 0.707f, 0.f);
        hpHigh_.set(maxiBiquad::filterTypes::HIGHPASS, 4000.f, 0.707f, 0.f);
        for (size_t b = 0; b < NBands; ++b) {
            env_[b].setAttack(5.f);
            env_[b].setRelease(50.f);
            prevNorm_[b] = 0.f;
            bandEnv[b] = 0.f;
            bandFlux[b] = 0.f;
        }
        counter_ = 0;
        fluxTotal = 0.f;
    }

    __force_inline void process(float x) {
        const float lo = lpLow_.play(x);
        const float md = lpMid_.play(hpMid_.play(x));  // bandpass = HP then LP
        const float hi = hpHigh_.play(x);

        bandEnv[0] = env_[0].play(lo);
        bandEnv[1] = env_[1].play(md);
        bandEnv[2] = env_[2].play(hi);

        // Difference at control rate, not per sample.
        if (++counter_ >= kHop) {
            counter_ = 0;
            const float total = bandEnv[0] + bandEnv[1] + bandEnv[2] + 1e-8f;
            float ftot = 0.f;
            for (size_t b = 0; b < NBands; ++b) {
                const float n = bandEnv[b] / total;
                const float d = n - prevNorm_[b];
                const float f = (d > 0.f) ? d : 0.f;   // half-wave rectify
                bandFlux[b] += kFluxSmooth * (f - bandFlux[b]);
                prevNorm_[b] = n;
                ftot += bandFlux[b];
            }
            fluxTotal += kFluxSmooth * (ftot - fluxTotal);
        }
    }

private:
    static constexpr size_t kHop = 64;          // ~750 Hz at 48k: flux time window
    static constexpr float  kFluxSmooth = 0.3f; // one-pole smoothing at hop rate

    maxiBiquad lpLow_, hpMid_, lpMid_, hpHigh_;
    maxiEnvelopeFollowerF env_[NBands];
    float prevNorm_[NBands] = {0};
    size_t counter_ = 0;
};

#endif // __SUBBANDFLUX_HPP__
