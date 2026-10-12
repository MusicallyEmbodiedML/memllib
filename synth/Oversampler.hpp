#ifndef __OVERSAMPLER_HPP__
#define __OVERSAMPLER_HPP__

#include <array>
#include <cmath>
#include <cstddef>

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

// ---------------------------------------------------------------------------
// Oversampler — reusable integer-factor oversampling for per-sample nonlinear
// processing (clipping, distortion, waveshaping ...).
//
// A nonlinearity generates harmonics above the input bandwidth; running it at
// the base rate aliases those harmonics back down. This wraps the standard
// fix: upsample -> apply the nonlinearity at the higher rate -> band-limit and
// decimate back down.
//
// Usage (one instance per audio channel — it holds filter state):
//
//     Oversampler<4> os;                 // 4x
//     float y = os.process(x, [](float s){ return clip(s); });
//
// The lambda is called Factor times per input sample, once per oversampled
// sample, and must return the processed value.
//
// Filtering is a single linear-phase windowed-sinc FIR (Blackman window),
// decomposed into polyphase form for the interpolation stage. The same
// prototype lowpass (cutoff = base Nyquist) is reused for decimation. Cost is
// ~2 * Factor * TapsPerPhase multiplies per input sample per channel.
// ---------------------------------------------------------------------------

template<size_t Factor, size_t TapsPerPhase = 8>
class Oversampler {
public:
    static constexpr size_t kFactor      = Factor;
    static constexpr size_t kTapsPerPhase = TapsPerPhase;
    static constexpr size_t kProtoLen    = Factor * TapsPerPhase;

    Oversampler() {
        designPrototype();
        reset();
    }

    void reset() {
        upHistory_.fill(0.f);
        downHistory_.fill(0.f);
    }

    // Process one input sample. fn is invoked Factor times (the oversampled
    // samples) and must return the processed value for each.
    template<typename Fn>
    __force_inline float process(float x, Fn&& fn) {
        // --- shift input history (newest at [0]) ---
        for (size_t m = TapsPerPhase - 1; m > 0; --m) {
            upHistory_[m] = upHistory_[m - 1];
        }
        upHistory_[0] = x;

        // --- interpolate, process, feed decimation history ---
        for (size_t k = 0; k < Factor; ++k) {
            float acc = 0.f;
            for (size_t m = 0; m < TapsPerPhase; ++m) {
                acc += proto_[k + m * Factor] * upHistory_[m];
            }
            // *Factor restores passband gain lost to zero-stuffing.
            const float up = acc * static_cast<float>(Factor);

            const float processed = fn(up);

            // push into decimation history (newest at [0])
            for (size_t j = kProtoLen - 1; j > 0; --j) {
                downHistory_[j] = downHistory_[j - 1];
            }
            downHistory_[0] = processed;
        }

        // --- band-limit + decimate: one output per Factor samples ---
        float out = 0.f;
        for (size_t j = 0; j < kProtoLen; ++j) {
            out += proto_[j] * downHistory_[j];
        }
        return out;
    }

private:
    std::array<float, kProtoLen> proto_;       // prototype lowpass (DC gain 1)
    std::array<float, TapsPerPhase> upHistory_; // last TapsPerPhase input samples
    std::array<float, kProtoLen> downHistory_;  // last kProtoLen oversampled samples

    // Windowed-sinc lowpass at cutoff = base Nyquist (1/Factor of the
    // oversampled Nyquist), normalised to unity DC gain.
    void designPrototype() {
        constexpr float kPi = 3.14159265358979323846f;
        const float center = (kProtoLen - 1) * 0.5f;
        const float cutoff = 1.f / static_cast<float>(Factor); // = 2 * fc/fs_os

        float sum = 0.f;
        for (size_t i = 0; i < kProtoLen; ++i) {
            const float n = static_cast<float>(i) - center;

            // ideal lowpass impulse: cutoff * sinc(cutoff * n)
            float sinc;
            if (n == 0.f) {
                sinc = cutoff;
            } else {
                const float a = kPi * cutoff * n;
                sinc = cutoff * sinf(a) / a;
            }

            // Blackman window
            const float w = 0.42f
                - 0.5f  * cosf(2.f * kPi * i / (kProtoLen - 1))
                + 0.08f * cosf(4.f * kPi * i / (kProtoLen - 1));

            proto_[i] = sinc * w;
            sum += proto_[i];
        }

        const float norm = 1.f / sum;
        for (size_t i = 0; i < kProtoLen; ++i) {
            proto_[i] *= norm;
        }
    }
};

#endif // __OVERSAMPLER_HPP__
