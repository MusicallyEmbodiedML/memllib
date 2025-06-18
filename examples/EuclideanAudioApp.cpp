#include "EuclideanAudioApp.hpp"
#include "../PicoDefs.hpp"


void EuclideanAudioApp::Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) {
    AudioAppBase::Setup(sample_rate, interface);
    // Additional setup code specific to EuclideanAudioApp
    phase_ = 0.0f;
    SetBPM(120.0f); // Default BPM
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

    // Process parameters for each operator
    for (size_t i = 0; i < kN_Operators; ++i) {
        // Get the relevant parameters for current operator
        std::vector<float> current_params(
            params.begin() + i * 3,
            params.begin() + (i + 1) * 3
        );
        VoiceOperator_(params_t.op_params[i],
                       current_params,
                       operator_voicing_[i]);
    }
}


void EuclideanAudioApp::Process(const stereosample_t x) {
    // Process audio with Euclidean rhythms
    phasor += phase_increment_;
    if (phasor >= 1.0f) {
        phasor -= 1.0f;
    }

    // Audio is passthrough
    stereosample_t output = x;

    std::vector<float> eucliean_output(kN_Operators);
    for (size_t i = 0; i < kN_Operators; ++i) {
        const auto& op_params = params_t.op_params[i];
        eucliean_output[i] = static_cast<float>(
            euclidean(phasor,
                      op_params.n,
                      op_params.k,
                      op_params.offset,
                      kPulseWidth)
        );
    }

    if (euclidean_callback_) {
        euclidean_callback_(eucliean_output);
    }

    return output;
}