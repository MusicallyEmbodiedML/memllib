#include "AudioAppBase.hpp"
#include <Arduino.h>


std::function<stereosample_t(stereosample_t)> AudioAppBase::callback_;

AudioAppBase::AudioAppBase() = default;
AudioAppBase::~AudioAppBase() = default;

stereosample_t AudioAppBase::Process(const stereosample_t x) {
    return x;
}

stereosample_t AudioAppBase::audioCallback(const stereosample_t x) {
    return callback_(x);
}

void AudioAppBase::Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) {
    sample_rate_ = sample_rate;
    interface_ = interface;
    callback_ = [this](stereosample_t x) { return Process(x); };
    AudioDriver::SetCallback(audioCallback);
}

void AudioAppBase::ProcessParams(const std::vector<float>& params) {
    // Default implementation does nothing
}

void AudioAppBase::loop() {
    if (!interface_) {
        Serial.println("AudioAppBase::loop - Error: Interface is null");
        return;  // Early return if interface is null
    }
    
    std::vector<float> x;
    if (interface_->ReceiveParamsFromQueue(x)) {
        ProcessParams(x);
    }
}
