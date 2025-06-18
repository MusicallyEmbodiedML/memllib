#ifndef __EUCLIDEAN_AUDIO_APP_HPP__
#define __EUCLIDEAN_AUDIO_APP_HPP__


#include <Arduino.h>
#include <cmath>
#include "../audio/AudioAppBase.hpp"


class EuclideanAudioApp : public AudioAppBase
{
public:

    // Constant values
    static constexpr float kPulseWidth = 0.05f;
    static constexpr size_t kN_Operators = 8;

    // Parameter definitions
    struct operator_params_t {
        size_t n;
        size_t k;
        size_t offset;
    };
    struct {
        operator_params_t op_params[kN_Operators];
    } params_t;
    static constexpr size_t kN_Params = sizeof(params_t) / sizeof(float);

    // Callback definition
    using euclidean_callback_t = std::function<void(const std::vector<float>&)>;

    // API
    EuclideanAudioApp : public AudioAppBase();
    ~EuclideanAudioApp : public AudioAppBase();

    stereosample_t Process(const stereosample_t x) override;
    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override;
    void ProcessParams(const std::vector<float>& params) override;
    inline void SetBPM(float bpm)
    {
        static constexpr float kOneOverSixty = 1.0f / 60.0f;
        // Calculate phase increment based on BPM
        phase_increment_ = (bpm * kOneOverSixty) * kSampleRateRcpr;
    }
    inline void SetCallback(euclidean_callback_t callback)
    {
        euclidean_callback_ = callback;
    }


protected:

    struct operator_voicing_t {
        size_t n_min;
        size_t n_max;
        size_t k_min;
        size_t k_max;
        // Offset is always limited to 0..n-1
    };

    operator_voicing_t operator_voicing_[kN_Operators] = {
        { 1, 16, 1, 16 }, // Operator 0
        { 1, 16, 1, 16 }, // Operator 1
        { 1, 16, 1, 16 }, // Operator 2
        { 1, 16, 1, 16 }, // Operator 3
        { 1, 16, 1, 16 }, // Operator 4
        { 1, 16, 1, 16 }, // Operator 5
        { 1, 16, 1, 16 }, // Operator 6
        { 1, 16, 1, 16 } // Operator 7
    };

    float phasor = 0.0f;
    float phase_increment_ = 0.0f;
    euclidean_callback_t euclidean_callback_ = nullptr;

    /**
     * @brief Map a float value [0..1] to a discrete range
     *
     * @param x Float input
     * @param min Minimum value of the range
     * @param max Maximum value of the range
     * @return size_t Discretised value in the range [min, max]
     */
    inline size_t DiscreteMap_(float x, size_t min, size_t max)
    {
        // Clamp x to [0, 1]
        x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        // Map to range [min, max] with proper rounding
        float mapped = static_cast<float>(min) + (x * static_cast<float>(max - min));
        return static_cast<size_t>(std::round(mapped));
    }
    /**
     * @brief Voice a single operator:
     *
     * @param op_params
     * @param params
     */
    inline void VoiceOperator_(operator_params_t& op_params, std::vector<float> &params, operator_voicing_t& voicing)
    {
        // Map n, k, offset from params
        op_params.n = DiscreteMap_(params[0], voicing.n_min, voicing.n_max);
        op_params.k = DiscreteMap_(params[1], voicing.k_min, voicing.k_max);
        op_params.offset = DiscreteMap_(params[2], 0, op_params.n - 1);

        // Ensure k is not greater than n
        if (op_params.k > op_params.n) {
            op_params.k = op_params.n;
        }
    }

    static bool __force_inline euclidean(float _phase, const size_t _n, const size_t _k, const size_t _offset, const float _pulseWidth)
    {
        // Euclidean function
        const float fi = _phase * _n;
        int i = static_cast<int>(fi);
        const float rem = fi - i;
        if (i == _n)
        {
            i--;
        }
        const int idx = ((i + _n - _offset) * _k) % _n;
        return (idx < _k && rem < _pulseWidth) ? 1 : 0;
    }
};


#endif  // __EUCLIDEAN_AUDIO_APP_HPP__
