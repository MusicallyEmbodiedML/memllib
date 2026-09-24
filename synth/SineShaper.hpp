#ifndef __SINESHAPER_HPP__
#define __SINESHAPER_HPP__

#include "Oversampler.hpp"
#include <cmath>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// SineShaper — 4x oversampled sine waveshaper, copied from MEMLCelium.
//
// The shaping core is the double-sine fold from MEMLCeliumAudioApp::Process:
//
//   shape = sin(x * 2pi);
//   shape = sin((shape * 2pi) * gain + asym);
//
// Folding the signal through sine repeatedly generates a rich, evolving
// harmonic series; `gain` sets the fold depth and `asym` is a phase offset that
// breaks symmetry (adding even harmonics + DC). Output is bounded to ±1. Sine
// folding is heavily harmonic, so oversampling matters more here than for the
// clippers. A DC blocker downstream is recommended when asym != 0.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class SineShaper {
public:
    void setGain(float gain) { gain_ = gain; }
    void setAsym(float asym) { asym_ = asym; }
    void reset()             { os_.reset(); }

    static __force_inline float shape(float x, float gain, float asym) {
        constexpr float kTwoPi = 6.28318530717958647692f;
        float s = sinf(x * kTwoPi);
        s = sinf(((s * kTwoPi) * gain) + asym);
        return s;
    }

    __force_inline float process(float x) {
        const float gain = gain_;
        const float asym = asym_;
        return os_.process(x, [gain, asym](float s) {
            return shape(s, gain, asym);
        });
    }

private:
    Oversampler<Factor> os_;
    float gain_{0.1f};  // MEMLCelium default
    float asym_{0.f};
};

#endif // __SINESHAPER_HPP__
