#ifndef __VARIABLEKNEECLIP_HPP__
#define __VARIABLEKNEECLIP_HPP__

#include "Oversampler.hpp"
#include <cmath>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// VariableKneeClip — 4x oversampled clipper with a continuously variable knee
// (one instance per channel).
//
//   drive : input gain before the curve.
//   knee  : 0 = soft (gentle, tanh-like), 1 = hard (sharp corners).
//
// One coherent curve spans the whole soft<->hard axis via a "hardness" k:
//
//   y = tanh(k * x) / tanh(k)
//
// Small k -> near-linear/soft; large k -> the curve flattens to ~sign(x) inside
// [-1, 1], i.e. effectively a hard clip. Normalising by tanh(k) keeps unity slope
// reachable at the rails. This gives the hard<->soft sweep on a single parameter
// instead of summing separate hard and soft clippers.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class VariableKneeClip {
public:
    void setDrive(float drive) { drive_ = drive; }
    void setKnee(float knee)   { knee_ = knee; }   // 0 soft .. 1 hard
    void reset()               { os_.reset(); }

    static __force_inline float clip(float x, float drive, float k) {
        const float kx = tanhf(k * x * drive);
        return kx / tanhf(k);
    }

    __force_inline float process(float x) {
        const float drive = drive_;
        // map knee 0..1 -> hardness k 1..50
        const float k = 1.f + knee_ * 49.f;
        return os_.process(x, [drive, k](float s) {
            return clip(s, drive, k);
        });
    }

private:
    Oversampler<Factor> os_;
    float drive_{1.f};
    float knee_{0.5f};
};

#endif // __VARIABLEKNEECLIP_HPP__
