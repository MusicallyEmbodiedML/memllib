#include "InterfaceBase.hpp"
#include <Arduino.h>


InterfaceBase::InterfaceBase() :
    init_done_(false),
    n_inputs_(0),
    n_outputs_(0)
{
}

InterfaceBase::~InterfaceBase()
{
}

void InterfaceBase::setup(size_t n_inputs, size_t n_outputs)
{
    queue_init(&queue_audioparam_, sizeof(float)*n_outputs, 1);
    n_inputs_ = n_inputs;
    n_outputs_ = n_outputs;
}

void InterfaceBase::SendParamsToQueue(const std::vector<float>& data) {
    if (data.size() != n_outputs_) {
        Serial.println("InterfaceBase::SendParamsToQueue - Error: data size mismatch");
        Serial.printf("Expected: %zu, Received: %zu\n", n_outputs_, data.size());
        return;
    }
    queue_try_add(&queue_audioparam_, data.data());
}

bool InterfaceBase::ReceiveParamsFromQueue(std::vector<float>& data) {
    if (data.size() != n_outputs_) {
        data.resize(n_outputs_);
    }
    return queue_try_remove(&queue_audioparam_, data.data());
}
