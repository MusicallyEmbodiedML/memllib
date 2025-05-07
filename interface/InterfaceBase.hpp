#ifndef __INTERFACE_BASE_HPP__
#define __INTERFACE_BASE_HPP__

#include "pico/util/queue.h"
#include <vector>
#include "UARTOutput.hpp"
#include "MIDI.hpp"
#include <memory>

class InterfaceBase
{
protected:
    InterfaceBase();

    // Member variables
    bool init_done_;
    size_t n_inputs_;
    size_t n_outputs_;
    queue_t queue_audioparam_;
    std::unique_ptr<UARTOutput> uart_output_;
    std::shared_ptr<MIDI> midi_;

public:
    ~InterfaceBase();

    // Disable copy constructor and assignment operator
    InterfaceBase(const InterfaceBase&) = delete;
    InterfaceBase& operator=(const InterfaceBase&) = delete;

    // Disable move constructor and assignment operator
    InterfaceBase(InterfaceBase&&) = delete;
    InterfaceBase& operator=(InterfaceBase&&) = delete;

    // Virtual functions with default implementations
    virtual void setup(size_t n_inputs, size_t n_outputs);

    void SetMIDIInterface(std::shared_ptr<MIDI> midi) {
        midi_ = midi;
    }

    // Queue management
    void SendParamsToQueue(const std::vector<float>& data);
    bool ReceiveParamsFromQueue(std::vector<float>& data);
};

#endif// __INTERFACE_BASE_HPP__
