#ifndef __ASYMPOWERSHAPER_HPP__
#define __ASYMPOWERSHAPER_HPP__

#include "Oversampler.hpp"
#include <cmath>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// AsymPowerShaper — 4x oversampled asymmetric power waveshaper.
//
// Raises the signal magnitude to a different exponent for the positive and
// negative halves, which breaks the waveform's symmetry and so introduces even
// harmonics (plus a DC offset) on top of the odd ones:
//
//   x >= 0 :  +|x|^posExp
//   x <  0 :  -|x|^negExp     (sign-preserving, with -3 < posExp,negExp < 3)
//
// pow() of a negative base with a fractional exponent is undefined, so the
// shaper operates on |x| and restores the sign. Input is clamped to [-1, 1]
// (the meaningful waveshaping domain); the magnitude is floored to avoid the
// pow(0, negative) -> inf singularity, and the output is bounded to [-1, 1] so
// negative (expanding) exponents can't blow up. A DC blocker downstream is
// recommended since asymmetry adds DC.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class AsymPowerShaper {
public:
    void setPosExp(float e) { posExp_ = e; }
    void setNegExp(float e) { negExp_ = e; }
    void reset()            { os_.reset(); }

    static __force_inline float shape(float x, float posExp, float negExp) {
        if (x > 1.f)       x = 1.f;
        else if (x < -1.f) x = -1.f;

        float m = fabsf(x);
        if (m < 1e-4f) m = 1e-4f;  // avoid pow(0, negative) == inf

        const float e = (x >= 0.f) ? posExp : negExp;
        float out = powf(m, e);
        if (out > 1.f) out = 1.f;  // bound expansion from negative exponents

        return (x >= 0.f) ? out : -out;
    }

    __force_inline float process(float x) {
        const float posExp = posExp_;
        const float negExp = negExp_;
        return os_.process(x, [posExp, negExp](float s) {
            return shape(s, posExp, negExp);
        });
    }

private:
    Oversampler<Factor> os_;
    float posExp_{1.f};
    float negExp_{1.f};
};

#endif // __ASYMPOWERSHAPER_HPP__
