#ifndef __HARDCLIP_HPP__
#define __HARDCLIP_HPP__

#include "Oversampler.hpp"

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// HardClip — 4x oversampled hard clipper (one instance per audio channel).
//
//   drive   : input gain applied before clipping (>1 pushes harder into the
//             ceiling, generating more harmonics)
//   ceiling : the clip threshold (output is bounded to [-ceiling, +ceiling])
//
// The clip itself is a memoryless nonlinearity; oversampling keeps the sharp
// corners from aliasing back into the audible band. Swap the lambda / static
// clip() for a different shaper to build other distortions on the same
// Oversampler.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class HardClip {
public:
    void setDrive(float drive)     { drive_ = drive; }
    void setCeiling(float ceiling) { ceiling_ = ceiling; }
    void reset()                   { os_.reset(); }

    static __force_inline float clip(float x, float ceiling) {
        if (x > ceiling)  return ceiling;
        if (x < -ceiling) return -ceiling;
        return x;
    }

    __force_inline float process(float x) {
        const float drive   = drive_;
        const float ceiling = ceiling_;
        return os_.process(x, [drive, ceiling](float s) {
            return clip(s * drive, ceiling);
        });
    }

private:
    Oversampler<Factor> os_;
    float drive_{1.f};
    float ceiling_{1.f};
};

#endif // __HARDCLIP_HPP__
