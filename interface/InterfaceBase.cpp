#include "InterfaceBase.hpp"
#include <Arduino.h>

InterfaceBase::InterfaceBase() :
    init_done_(false),
    n_inputs_(0),
    n_outputs_(0),
    n_directparams_(0),
    n_audioanalysisparams_(0)
{
}

InterfaceBase::~InterfaceBase()
{
}

void InterfaceBase::setup(size_t n_inputs, size_t n_outputs, size_t n_directparams, size_t n_audioanalysisparams) {
    queue_init(&queue_audioparam_, sizeof(float)*n_outputs, 1);
    queue_init(&queue_directparam_, sizeof(float)*n_directparams, 1);
    queue_init(&queue_analysisparam_, sizeof(float)*n_audioanalysisparams, 1);
    
    n_inputs_ = n_inputs;
    n_outputs_ = n_outputs;
    n_directparams_ = n_directparams;
    n_audioanalysisparams_ = n_audioanalysisparams;

    init_done_ = true;
    Serial.printf("InterfaceBase::setup - Initialized with %zu inputs, %zu outputs, %zu direct params, %zu analysis params\n",
                  n_inputs_, n_outputs_, n_directparams_, n_audioanalysisparams_);
}

void InterfaceBase::SendToQueue(const std::vector<float>& data, queue_t& queue, size_t expected_size, const char* method_name) {
    if (!init_done_) {
        Serial.printf("%s - Error: Interface not initialized\n", method_name);
        return;
    }
    if (expected_size == 0) {
        return;
    }
    if (data.size() != expected_size) {
        Serial.printf("%s - Error: data size mismatch\n", method_name);
        Serial.printf("Expected: %zu, Received: %zu\n", expected_size, data.size());
        return;
    }
    queue_try_add(&queue, data.data());
}

bool InterfaceBase::ReceiveFromQueue(std::vector<float>& data, queue_t& queue, size_t expected_size, const char* method_name) {
    if (!init_done_) {
        Serial.printf("%s - Error: Interface not initialized\n", method_name);
        return false;
    }
    if (expected_size == 0) {
        return false;
    }
    if (data.size() != expected_size) {
        data.resize(expected_size);
    }
    return queue_try_remove(&queue, data.data());
}

void InterfaceBase::SendParamsToQueue(const std::vector<float>& data) {
    SendToQueue(data, queue_audioparam_, n_outputs_, "InterfaceBase::SendParamsToQueue");
}

bool InterfaceBase::ReceiveParamsFromQueue(std::vector<float>& data) {
    return ReceiveFromQueue(data, queue_audioparam_, n_outputs_, "InterfaceBase::ReceiveParamsFromQueue");
}

void InterfaceBase::SendDirectParamsToQueue(const std::vector<float>& data) {
    SendToQueue(data, queue_directparam_, n_directparams_, "InterfaceBase::SendDirectParamsToQueue");
}

bool InterfaceBase::ReceiveDirectParamsFromQueue(std::vector<float>& data) {
    return ReceiveFromQueue(data, queue_directparam_, n_directparams_, "InterfaceBase::ReceiveDirectParamsFromQueue");
}

void InterfaceBase::SendAnalysisParamsToQueue(const std::vector<float>& data) {
    SendToQueue(data, queue_analysisparam_, n_audioanalysisparams_, "InterfaceBase::SendAnalysisParamsToQueue");
}

bool InterfaceBase::ReceiveAnalysisParamsFromQueue(std::vector<float>& data) {
    return ReceiveFromQueue(data, queue_analysisparam_, n_audioanalysisparams_, "InterfaceBase::ReceiveAnalysisParamsFromQueue");
}