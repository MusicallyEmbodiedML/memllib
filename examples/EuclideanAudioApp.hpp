#ifndef __EUCLIDEAN_AUDIO_APP_HPP__
#define __EUCLIDEAN_AUDIO_APP_HPP__


#include <Arduino.h>
#include <cmath>
#include "../audio/AudioAppBase.hpp"


static_assert(sizeof(size_t) == sizeof(float),
              "size_t and float must have the same size for automatic parameter counting");


class EuclideanAudioApp : public AudioAppBase
{
public:

    // Constant values
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
    EuclideanAudioApp() : AudioAppBase() {};

    stereosample_t Process(const stereosample_t x) override;
    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override;
    void ProcessParams(const std::vector<float>& params) override;
    inline void SetBPM(float bpm)
    {
        // Validate BPM range
        if (bpm <= 0.0f || bpm > 999.0f) {
            return; // Ignore invalid BPM values
        }
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

    float phase_ = 0.0f;
    float phase_increment_ = 0.0f;
    euclidean_callback_t euclidean_callback_ = nullptr;
    float previous_euclidean_output_[kN_Operators] = {0.0f}; ///< Previous output values for change detection

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
        // Handle edge cases
        if (min > max) {
            return min; // Return min if range is invalid
        }
        if (min == max) {
            return min; // Return the single value
        }

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
        size_t local_k_max = std::min(voicing.k_max, op_params.n); // Ensure k does not exceed n
        op_params.k = DiscreteMap_(params[1], voicing.k_min, local_k_max);

        // Ensure n is at least 1 before calculating offset range
        if (op_params.n > 0) {
            op_params.offset = DiscreteMap_(params[2], 0, op_params.n - 1);
        } else {
            op_params.offset = 0;
        }

        // Ensure k is not greater than n
        if (op_params.k > op_params.n) {
            op_params.k = op_params.n;
        }

        // Serial.print("Operator params: n=");
        // Serial.print(op_params.n);
        // Serial.print(", k=");
        // Serial.print(op_params.k);
        // Serial.print(", offset=");
        // Serial.println(op_params.offset);
    }

    static bool __force_inline euclidean(float _phase, const size_t _n, const size_t _k, const size_t _offset, const float _pulseWidth)
    {
        if (_n == 0 || _k == 0 || _k > _n) {
            return false;
        }

        // Convert phase to step index
        const float fi = _phase * static_cast<float>(_n);
        size_t step = static_cast<size_t>(fi);
        const float rem = fi - static_cast<float>(step);

        if (step >= _n) {
            step = _n - 1;
        }

        // Apply offset
        step = (step + _offset) % _n;

        // Simple Euclidean rhythm using integer arithmetic
        // This gives the correct Euclidean distribution
        const bool is_pulse_step = ((step * _k) / _n) != (((step - 1 + _n) % _n) * _k) / _n;

        return (is_pulse_step && rem < _pulseWidth);
    }

    /**
     * @brief Check if euclidean output has changed since last call
     * @param current_output Current euclidean output values (fixed-size array)
     * @return true if any value has changed, false otherwise
     */
    inline bool HasOutputChanged_(const float current_output[kN_Operators]) {
        bool changed = false;
        // Update all values and check for changes in single pass
        for (size_t i = 0; i < kN_Operators; ++i) {
            if (current_output[i] != previous_euclidean_output_[i]) {
                previous_euclidean_output_[i] = current_output[i];
                changed = true;
            }
        }
        return changed;
    }
};


#endif  // __EUCLIDEAN_AUDIO_APP_HPP__
