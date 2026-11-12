#ifndef __FASTMATH_HPP__
#define __FASTMATH_HPP__

#include <cstdint>
#include <cstring>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// FastMath — inline float approximations for the per-sample audio path.
//
// On the MEMLNaut (RP2350, Arduino-Pico), sinf/tanhf/expf/exp2f/powf/fmodf
// resolve to the Pico SDK's wrapped float routines, which live in flash. The
// audio callback runs from SRAM, so every call is an XIP round trip, and it
// contends with core 0 (the neural network) for the flash cache. In LiveFX,
// replacing six such calls per sample took the worst-case audio load from
// ~911 to ~487 us per 1000 us block.
//
// These are accurate enough to be drop-in replacements in sound-producing
// code (not just modulation):
//   sinTurns / sin : max abs error ~4e-6
//   exp2 / exp     : max rel error ~3e-7 (inputs clamped to +-126 octaves)
//   tanh           : max abs error ~2e-7
//   log2           : max abs error ~2e-4 (x > 0, normal floats)
//   fmod1          : exact, = fmodf(x, 1.f) for |x| < 2^31
// ---------------------------------------------------------------------------
namespace FastMath {

    // sin(2*pi*t), t in turns, any value: reduce to [-0.5, 0.5), fold to the
    // quarter wave, odd 9th-order Taylor.
    static __force_inline float sinTurns(float t) {
        const float u = t + 0.5f;
        int32_t i = static_cast<int32_t>(u);
        if (u < static_cast<float>(i)) --i;           // floor
        float r = t - static_cast<float>(i);          // [-0.5, 0.5)
        if (r > 0.25f) r = 0.5f - r;                  // fold to [-0.25, 0.25]
        else if (r < -0.25f) r = -0.5f - r;
        const float x = r * 6.28318530718f;           // [-pi/2, pi/2]
        const float x2 = x * x;
        return x * (1.f + x2 * (-1.6666667e-1f + x2 * (8.3333333e-3f
                 + x2 * (-1.9841270e-4f + x2 * 2.7557319e-6f))));
    }

    // sin(x), x in radians.
    static __force_inline float sin(float x) { return sinTurns(x * 0.159154943092f); }

    // 2^x: integer part into the exponent bits, fraction by a 5th-order fit.
    static __force_inline float exp2(float x) {
        if (x < -126.f) x = -126.f;
        else if (x > 126.f) x = 126.f;
        int32_t i = static_cast<int32_t>(x);
        if (x < static_cast<float>(i)) --i;           // floor
        const float f = x - static_cast<float>(i);    // [0, 1)
        const float p = 9.999997696e-01f + f * (6.931567767e-01f + f * (2.401316919e-01f
                      + f * (5.587655687e-02f + f * (8.940582529e-03f + f * 1.894379423e-03f))));
        int32_t bits;
        std::memcpy(&bits, &p, sizeof(bits));
        bits += i << 23;
        float out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    // e^x
    static __force_inline float exp(float x) { return exp2(x * 1.44269504089f); }

    // log2(x) for x > 0: exponent bits plus a quartic fit of log2 on the mantissa [1, 2).
    static __force_inline float log2(float x) {
        int32_t bits;
        std::memcpy(&bits, &x, sizeof(bits));
        const float e = static_cast<float>(((bits >> 23) & 0xff) - 127);
        bits = (bits & 0x007fffff) | 0x3f800000;
        float m;
        std::memcpy(&m, &bits, sizeof(m));
        return e + (-2.49684606f + m * (4.02854794f + m * (-2.08121416f
                 + m * (0.62887362f - 0.07915816f * m))));
    }

    // tanh(x) = sign(x) * (1 - e^-2|x|) / (1 + e^-2|x|)
    static __force_inline float tanh(float x) {
        const float a = x < 0.f ? -x : x;
        if (a > 9.f) return x < 0.f ? -1.f : 1.f;     // tanh(9) = 1 - 3e-8
        const float e = exp2(a * -2.88539008178f);    // e^(-2a)
        const float t = (1.f - e) / (1.f + e);
        return x < 0.f ? -t : t;
    }

    // fmodf(x, 1.f): fractional part with the sign of x.
    static __force_inline float fmod1(float x) {
        return x - static_cast<float>(static_cast<int32_t>(x));
    }

}  // namespace FastMath

#endif  // __FASTMATH_HPP__
