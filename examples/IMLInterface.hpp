#ifndef __IML_INTERFACE_HPP__
#define __IML_INTERFACE_HPP__

#include <Arduino.h>
#include "../interface/InterfaceBase.hpp"
#include <memory>
#include "../../memlp/MLP.h"
#include "../../memlp/Dataset.hpp"


// Forward declarations
class Dataset;
template<typename T> class MLP;
class UARTInput;
class MIDIInOut;
class display;

class IMLInterface : public InterfaceBase
{
public:
    IMLInterface() : InterfaceBase() {}

    void setup(size_t n_inputs, size_t n_outputs) override;
    void setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp);

    enum training_mode_t {
        INFERENCE_MODE,
        TRAINING_MODE
    };

    void SetTrainingMode(training_mode_t training_mode);
    void ProcessInput();
    void SetInput(size_t index, float value);

    enum saving_mode_t {
        STORE_VALUE_MODE,
        STORE_POSITION_MODE,
    };

    void SaveInput(saving_mode_t mode);
    void ClearData();
    void Randomise();
    void SetIterations(size_t iterations);

    // New binding methods
    void bindInterface();
    void bindUARTInput(std::shared_ptr<UARTInput> uart_input, const std::vector<size_t>& kUARTListenInputs);
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf);

protected:
    size_t n_inputs_;
    size_t n_outputs_;
    size_t n_iterations_;

    // State machine
    training_mode_t training_mode_;
    bool perform_inference_;
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

    void MLSetup_();
    void MLInference_(std::vector<float> input);
    void MLRandomise_();
    void MLTraining_();
};

#endif  // __IML_INTERFACE_HPP__