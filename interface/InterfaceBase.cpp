#include "InterfaceBase.hpp"
#include <Arduino.h>


InterfaceBase::InterfaceBase() :
    init_done_(false),
    n_inputs_(0),
    n_outputs_(0),
    n_directparams_(0)
{
}

InterfaceBase::~InterfaceBase()
{
}

void InterfaceBase::setup(size_t n_inputs, size_t n_outputs, size_t n_directparams)
{
    queue_init(&queue_audioparam_, sizeof(float)*n_outputs, 1);
    queue_init(&queue_directparam_, sizeof(float)*n_directparams_, 1);
    n_inputs_ = n_inputs;
    n_outputs_ = n_outputs;
    n_directparams_ = n_directparams;

    init_done_ = true;
    Serial.printf("InterfaceBase::setup - Initialized with %zu inputs, %zu outputs, %zu direct params\n",
                  n_inputs_, n_outputs_, n_directparams_);
}

void InterfaceBase::setup(size_t n_inputs, size_t n_outputs)
{
    setup(n_inputs, n_outputs, 0);
}

void InterfaceBase::SendParamsToQueue(const std::vector<float>& data) {
    if (init_done_) {
        if (data.size() != n_outputs_) {
            Serial.println("InterfaceBase::SendParamsToQueue - Error: data size mismatch");
            Serial.printf("Expected: %zu, Received: %zu\n", n_outputs_, data.size());
            return;
        }
        queue_try_add(&queue_audioparam_, data.data());
    } else {
        Serial.println("InterfaceBase::SendParamsToQueue - Error: Interface not initialized");
    }
}

bool InterfaceBase::ReceiveParamsFromQueue(std::vector<float>& data) {
    if (!init_done_) {
        Serial.println("InterfaceBase::ReceiveParamsFromQueue - Error: Interface not initialized");
        return false;
    }
    if (data.size() != n_outputs_) {
        data.resize(n_outputs_);
    }
    return queue_try_remove(&queue_audioparam_, data.data());
}

bool InterfaceBase::ReceiveDirectParamsFromQueue(std::vector<float>& data) {
    if (!init_done_) {
        Serial.println("InterfaceBase::ReceiveDirectParamsFromQueue - Error: Interface not initialized");
        return false;
    }
    if (n_directparams_ == 0) {
        return false;
    }
    if (data.size() != n_directparams_) {
        data.resize(n_directparams_);
    }
    return queue_try_remove(&queue_directparam_, data.data());
}

void InterfaceBase::SendDirectParamsToQueue(const std::vector<float>& data) {
    if (init_done_) {
        if (n_directparams_ == 0) {
            return;
        }
        if (data.size() != n_directparams_) {
            Serial.println("InterfaceBase::SendDirectParamsToQueue - Error: data size mismatch");
            Serial.printf("Expected: %zu, Received: %zu\n", n_directparams_, data.size());
            return;
        }
        queue_try_add(&queue_directparam_, data.data());
    } else {
        Serial.println("InterfaceBase::SendDirectParamsToQueue - Error: Interface not initialized");
    }
}
