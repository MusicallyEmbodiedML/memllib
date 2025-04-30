#ifndef __INTERFACE_BASE_HPP__
#define __INTERFACE_BASE_HPP__

#include "pico/util/queue.h"
#include <vector>

class InterfaceBase
{
protected:
    InterfaceBase();

    // Member variables
    bool init_done_;
    size_t n_inputs_;
    size_t n_outputs_;
    size_t n_directparams_;
    queue_t queue_audioparam_;
    queue_t queue_directparam_;

public:
    ~InterfaceBase();

    // Disable copy constructor and assignment operator
    InterfaceBase(const InterfaceBase&) = delete;
    InterfaceBase& operator=(const InterfaceBase&) = delete;

    // Disable move constructor and assignment operator
    InterfaceBase(InterfaceBase&&) = delete;
    InterfaceBase& operator=(InterfaceBase&&) = delete;

    // Virtual functions with default implementations
    virtual void setup(size_t n_inputs, size_t n_outputs,
        size_t n_directparams);
    virtual void setup(size_t n_inputs, size_t n_outputs);

    // Queue management
    void SendParamsToQueue(const std::vector<float>& data);
    bool ReceiveParamsFromQueue(std::vector<float>& data);
    void SendDirectParamsToQueue(const std::vector<float>& data);
    bool ReceiveDirectParamsFromQueue(std::vector<float>& data);
};

#endif// __INTERFACE_BASE_HPP__
