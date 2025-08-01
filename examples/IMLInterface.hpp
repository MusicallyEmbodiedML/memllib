#ifndef __IML_INTERFACE_HPP__
#define __IML_INTERFACE_HPP__

#include <Arduino.h>
#include "../interface/InterfaceBase.hpp"
#include <memory>
#include "../../memlp/MLP.h"
#include "../../memlp/Dataset.hpp"
#include "../hardware/memlnaut/display/MessageView.hpp"

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
    // void setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp);

    enum training_mode_t {
        INFERENCE_MODE,
        TRAINING_MODE
    };

    bool SetTrainingMode(training_mode_t training_mode);
    void ProcessInput();
    void SetInput(size_t index, float value);

    enum saving_mode_t {
        STORE_VALUE_MODE,
        STORE_POSITION_MODE
    };

    bool SaveInput(saving_mode_t mode);
    bool ClearData();
    bool Randomise();
    void SetIterations(size_t iterations);
    void SetZoomEnabled(bool enabled);
    void SetZoomFactor(float factor) { zoom_factor_ = factor; }

    // New binding methods
    void bindInterface(bool disable_joystick = false);
    void bindUARTInput(std::shared_ptr<UARTInput> uart_input, const std::vector<size_t>& kUARTListenInputs);
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf);

    bool sdtest=false;

protected:
    static constexpr float kMax = 2.0f;
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
    std::shared_ptr<MessageView> msgView;

    // Zooming
    bool zoom_enabled_;
    float zoom_factor_;
    std::vector<float> zoom_centre_;

    void MLSetup_();
    void MLInference_(std::vector<float> input);
    void MLRandomise_();
    bool MLTraining_();
    std::vector<float> ZoomCoordinates(const std::vector<float>& coord, const std::vector<float>& zoom_centre, float factor);
};

#endif  // __IML_INTERFACE_HPP__