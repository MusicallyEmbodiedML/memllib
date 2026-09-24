#ifndef __CHEBYSHEVSHAPER_HPP__
#define __CHEBYSHEVSHAPER_HPP__

#include "Oversampler.hpp"
#include <array>
#include <cstddef>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// ChebyshevShaper — 4x oversampled Chebyshev waveshaper (one per channel).
//
// Chebyshev polynomials of the first kind satisfy Tn(cos t) = cos(n*t), so for a
// unit-amplitude input Tn produces exactly the n-th harmonic. A weighted sum
//
//     y = sum_n gain[n] * Tn(x)
//
// is therefore an additive "harmonic mixer": each gain dials the amount of one
// overtone. Here the gains drive harmonics T2..T7 (octave up to ~2 octaves + a
// fifth); the fundamental (T1) is omitted since the dry signal supplies it.
//
// The Tn -> n-th-harmonic identity is exact only at unit amplitude, so input is
// clamped to [-1, 1] (also keeps the recurrence bounded); at lower levels the
// harmonics blend, which is musically useful. Even-order terms (T2,T4,T6) add
// DC, so block DC downstream. Bandwidth scales with the highest order, hence the
// oversampling. Cost is one multiply-add per harmonic per oversampled sample.
// Output is scaled by 1/kNumHarmonics so all-gains-max stays ~unity.
// ---------------------------------------------------------------------------

template<size_t Factor = 4>
class ChebyshevShaper {
public:
    static constexpr size_t kNumHarmonics = 6;  // T2 .. T7

    void setGain(size_t i, float g) { if (i < kNumHarmonics) gains_[i] = g; }
    void reset()                    { os_.reset(); }

    __force_inline float process(float x) {
        const float* gains = gains_.data();
        return os_.process(x, [gains](float s) {
            if (s > 1.f)       s = 1.f;
            else if (s < -1.f) s = -1.f;

            // recurrence: T0=1, T1=x, Tn = 2x*T(n-1) - T(n-2)
            float tPrev = 1.f;  // T0
            float tCurr = s;    // T1
            float acc = 0.f;
            for (size_t n = 2; n <= kNumHarmonics + 1; ++n) {
                const float t = 2.f * s * tCurr - tPrev;  // Tn
                acc += gains[n - 2] * t;                   // gains[0]->T2 ...
                tPrev = tCurr;
                tCurr = t;
            }
            return acc * (1.f / static_cast<float>(kNumHarmonics));
        });
    }

private:
    Oversampler<Factor> os_;
    std::array<float, kNumHarmonics> gains_{};
};

#endif // __CHEBYSHEVSHAPER_HPP__
