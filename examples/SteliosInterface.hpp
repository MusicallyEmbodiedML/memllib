#ifndef __STELIOS_INTERFACE_HPP__
#define __STELIOS_INTERFACE_HPP__

#include <Arduino.h>
#include "../interface/InterfaceBase.hpp"
#include <memory>
#include "../../memlp/MLP.h"
#include "../../memlp/Dataset.hpp"
#include "../interface/SerialUSBInput.hpp"
#include "../interface/SerialUSBOutput.hpp"


// Forward declarations
class Dataset;
template<typename T> class MLP;
class UARTInput;
class MIDIInOut;
class display;

class SteliosInterface : public InterfaceBase
{
public:
    SteliosInterface() : InterfaceBase() {}

    void setup(size_t n_inputs, size_t n_outputs) override;
    void setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp);

    void ProcessInput();
    void SetInput(size_t index, float value);

    bool SaveInput(std::vector<float> category);
    bool Train();
    bool ClearData();
    bool Randomise();
    void SetIterations(size_t iterations);
    void SetZoomEnabled(bool enabled);
    void SetZoomFactor(float factor) { zoom_factor_ = factor; }

    // New binding methods
    void bindInterface();
    void bindUARTInput(std::shared_ptr<SerialUSBInput> uart_input, const std::vector<size_t>& kUARTListenInputs);
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf);

protected:
    static constexpr float kMax = 2.0f;
    size_t n_inputs_;
    size_t n_outputs_;
    size_t n_iterations_;

    // State machine
    volatile bool perform_inference_;
    bool input_updated_;

    // Controls/sensors
    std::vector<float> input_state_;
    std::vector<float> output_state_;

    // MLP core
    std::unique_ptr<Dataset> dataset_;
    std::unique_ptr<MLP<float>> mlp_;
    MLP<float>::mlp_weights mlp_stored_weights_;
    bool randomised_state_;

    // Display reference for binding methods
    std::shared_ptr<display> disp_;

    // Zooming
    bool zoom_enabled_;
    float zoom_factor_;
    std::vector<float> zoom_centre_;

    // Category selection
    size_t selected_category_;
    std::vector<size_t> class_occurrences_;

    void MLSetup_();
    void MLInference_(std::vector<float> input);
    void MLRandomise_();
    bool MLTraining_();
    std::vector<float> ZoomCoordinates(const std::vector<float>& coord, const std::vector<float>& zoom_centre, float factor);

    std::shared_ptr<SerialUSBOutput> uart_output;
};

#endif  // __STELIOS_INTERFACE_HPP__