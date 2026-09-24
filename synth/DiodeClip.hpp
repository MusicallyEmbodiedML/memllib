#ifndef __DIODECLIP_HPP__
#define __DIODECLIP_HPP__

#include "Oversampler.hpp"
#include <cmath>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// DiodeClip — 4x oversampled asymmetric diode-style clipper (one per channel).
//
//   drive       : input gain before the curve.
//   etaPos/etaNeg : "softness" of each half (smaller = sharper/harder).
//
// Models the exponential I/V curve of a diode clipper, saturating each half of
// the waveform independently:
//
//   x >= 0 :  +(1 - exp(-x / etaPos))
//   x <  0 :  -(1 - exp( x / etaNeg))
//
// Different eta per half breaks symmetry the way a real (mismatched) diode pair
// does, producing the characteristic fuzzy "tube/diode" even-harmonic content
// (plus DC, so DC-block downstream). Output is bounded to (-1, 1).
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class DiodeClip {
public:
    void setDrive(float drive)  { drive_ = drive; }
    void setEtaPos(float eta)   { etaPos_ = eta; }
    void setEtaNeg(float eta)   { etaNeg_ = eta; }
    void reset()                { os_.reset(); }

    static __force_inline float clip(float x, float etaPos, float etaNeg) {
        if (x >= 0.f) return 1.f - expf(-x / etaPos);
        else          return -(1.f - expf(x / etaNeg));
    }

    __force_inline float process(float x) {
        const float drive  = drive_;
        const float etaPos = etaPos_;
        const float etaNeg = etaNeg_;
        return os_.process(x, [drive, etaPos, etaNeg](float s) {
            return clip(s * drive, etaPos, etaNeg);
        });
    }

private:
    Oversampler<Factor> os_;
    float drive_{1.f};
    float etaPos_{0.5f};
    float etaNeg_{0.5f};
};

#endif // __DIODECLIP_HPP__
