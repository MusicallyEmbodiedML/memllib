#ifndef __TANHSOFTCLIP_HPP__
#define __TANHSOFTCLIP_HPP__

#include "Oversampler.hpp"
#include <cmath>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// TanhSoftClip — 4x oversampled tanh saturator (one instance per channel).
//
//   drive : input gain before the tanh. Higher drive = softer-knee saturation
//           that progressively approaches a square. Output is bounded to ~±1.
//
// Same memoryless-nonlinearity + Oversampler pattern as HardClip; tanh's gentle
// knee adds mostly lower-order harmonics where hard clipping adds harsh ones.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class TanhSoftClip {
public:
    void setDrive(float drive) { drive_ = drive; }
    void reset()               { os_.reset(); }

    __force_inline float process(float x) {
        const float drive = drive_;
        return os_.process(x, [drive](float s) {
            return tanhf(s * drive);
        });
    }

private:
    Oversampler<Factor> os_;
    float drive_{1.f};
};

#endif // __TANHSOFTCLIP_HPP__
