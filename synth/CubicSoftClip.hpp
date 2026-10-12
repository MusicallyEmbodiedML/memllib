#ifndef __CUBICSOFTCLIP_HPP__
#define __CUBICSOFTCLIP_HPP__

#include "Oversampler.hpp"
#include "maximilian.h"  // maxiNonlinearity::softclip

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// CubicSoftClip — 4x oversampled cubic soft clipper (one per channel).
//
//   drive : input gain before the curve.
//
// The classic "amp" soft clip: (2/3)(x - x^3/3) in the linear region, clamped to
// ±1 beyond. Gentler knee than tanh, mostly low-order odd harmonics. Reuses the
// codebase implementation maxiNonlinearity::softclip (maximilian.h).
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class CubicSoftClip {
public:
    void setDrive(float drive) { drive_ = drive; }
    void reset()               { os_.reset(); }

    __force_inline float process(float x) {
        const float drive = drive_;
        return os_.process(x, [drive](float s) {
            maxiNonlinearity nl;
            return nl.softclip(s * drive);
        });
    }

private:
    Oversampler<Factor> os_;
    float drive_{1.f};
};

#endif // __CUBICSOFTCLIP_HPP__
