#include "EuclideanAudioApp.hpp"
#include "../PicoDefs.hpp"


void EuclideanAudioApp::Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) {
    AudioAppBase::Setup(sample_rate, interface);
    // Additional setup code specific to EuclideanAudioApp
    phase_ = 0.0f;
    SetBPM(20.0f); // Default BPM
}


void EuclideanAudioApp::ProcessParams(const std::vector<float>& params) {
    // Ensure parameter size is right
    if (params.size() != kN_Params) {
        Serial.print("Error: Incorrect number of parameters provided.");
        Serial.print(" Expected: ");
        Serial.print(kN_Params);
        Serial.print(", Got: ");
        Serial.println(params.size());
        return;
    }

    // Calculate the number of parameters per operator based on operator_params_t
    constexpr size_t params_per_operator = sizeof(operator_params_t) / sizeof(float);

    // Process parameters for each operator
    for (size_t i = 0; i < kN_Operators; ++i) {
        // Get the relevant parameters for current operator
        std::vector<float> current_params(
            params.begin() + i * params_per_operator,
            params.begin() + (i + 1) * params_per_operator
        );
        VoiceOperator_(params_t.op_params[i],
                       current_params,
                       operator_voicing_[i]);
    }
}


stereosample_t EuclideanAudioApp::Process(const stereosample_t x) {
    // Process audio with Euclidean rhythms
    phase_ += phase_increment_;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }

    // Audio is passthrough
    stereosample_t output = x;

    // Use stack-allocated array instead of vector for efficiency
    float euclidean_output[kN_Operators];
    for (size_t i = 0; i < kN_Operators; ++i) {
        const auto& op_params = params_t.op_params[i];
        // Pulse width: half a step (1/(2*N)), always a submultiple of N
        float pulse_width = 0.5f;
        euclidean_output[i] = static_cast<float>(
            euclidean(phase_,
                      op_params.n,
                      op_params.k,
                      op_params.offset,
                      pulse_width)
        );
    }

    // Only call callback if values have changed
    if (euclidean_callback_ && HasOutputChanged_(euclidean_output)) {
        // Convert to vector only when callback is actually called
        std::vector<float> output_vector(euclidean_output, euclidean_output + kN_Operators);
        euclidean_callback_(output_vector);
    }

    return output;
}