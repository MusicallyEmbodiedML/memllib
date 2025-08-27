#include "IMLInterface.hpp"
#include "../../memlp/Dataset.hpp"
#include "../../memlp/MLP.h"
#include "../hardware/memlnaut/MEMLNaut.hpp"
#include "../interface/UARTInput.hpp"
#include "../interface/MIDIInOut.hpp"
#include "../hardware/memlnaut/display.hpp"
#include <algorithm>
#include <SD.h>


void IMLInterface::setup(size_t n_inputs, size_t n_outputs)
{
    InterfaceBase::setup(n_inputs, n_outputs);
    // Additional setup code specific to IMLInterface
    n_inputs_ = n_inputs;
    n_outputs_ = n_outputs;

    MLSetup_();
    n_iterations_ = 1000;
    input_state_.resize(n_inputs, 0.5f);
    output_state_.resize(n_outputs, 0);
    // Init/reset state machine
    training_mode_ = INFERENCE_MODE;
    perform_inference_ = true;
    input_updated_ = false;

    zoom_centre_ = std::vector<float>(n_inputs_, 0.5f);
    zoom_enabled_ = false;
    zoom_factor_ = 0.5f;

     DEBUG_PRINTLN("IMLInterface setup done");
     DEBUG_PRINT("Address of n_inputs_: ");
     DEBUG_PRINTLN(reinterpret_cast<uintptr_t>(&n_inputs_));
     DEBUG_PRINT("Inputs: ");
     DEBUG_PRINT(n_inputs_);
     DEBUG_PRINT(", Outputs: ");
     DEBUG_PRINTLN(n_outputs_);
}

void IMLInterface::setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp)
{
    setup(n_inputs, n_outputs);
    disp_ = disp;
}

bool IMLInterface::SetTrainingMode(training_mode_t training_mode)
{
     DEBUG_PRINT("Training mode: ");
     DEBUG_PRINTLN(training_mode == INFERENCE_MODE ? "Inference" : "Training");

    bool has_trained = false;

    if (training_mode == INFERENCE_MODE && training_mode_ == TRAINING_MODE) {
        // Train the network!
        has_trained = MLTraining_();
    }
    training_mode_ = training_mode;

    return has_trained;
}

void IMLInterface::ProcessInput()
{
    // Check if input is updated
    if (perform_inference_ && input_updated_) {
         //DEBUG_PRINT("Input state: ");
        // for (auto val : input_state_) {
        //     DEBUG_PRINT(val);
        //     DEBUG_PRINT(" ");
        // }
        // DEBUG_PRINTLN();
        // Only zoom here
        std::vector<float> zoomed_input = input_state_;
        if (zoom_enabled_) {
            zoomed_input = ZoomCoordinates(input_state_, zoom_centre_, zoom_factor_);
             DEBUG_PRINT("Zoomed input: ");
            // for (auto val : zoomed_input) {
            //     DEBUG_PRINT(val);
            //     DEBUG_PRINT(" ");
            // }
             DEBUG_PRINTLN();
        }
        MLInference_(zoomed_input);
        input_updated_ = false;
    }
}

void IMLInterface::SetInput(size_t index, float value)
{
    //  DEBUG_PRINT("Input ");
    //  DEBUG_PRINT(index);
    //  DEBUG_PRINT(" set to: ");
    //  DEBUG_PRINTLN(value);

    if (index >= n_inputs_) {
         DEBUG_PRINT("Input index ");
         DEBUG_PRINT(index);
         DEBUG_PRINTLN(" out of bounds.");
        return;
    }

    if (value < 0) {
        value = 0;
    } else if (value > 1.0) {
        value = 1.0;
    }
    value *= kMax; // Scale to kMax

    // Update state of input
    input_state_[index] = value;
    input_updated_ = true;
}

bool IMLInterface::SaveInput(saving_mode_t mode)
{
    if (training_mode_ == TRAINING_MODE) {
        if (STORE_VALUE_MODE == mode && perform_inference_) {

             DEBUG_PRINTLN("Move input to position...");
            perform_inference_ = false;
            return true;

        } else if (STORE_POSITION_MODE == mode && !perform_inference_) {

             DEBUG_PRINTLN("Creating example in this position.");
            // Save pair in the dataset
            dataset_->Add(input_state_, output_state_);
            perform_inference_ = true;
            MLInference_(input_state_);
            return true;
        }
        return false;
    } else {
         DEBUG_PRINTLN("Switch to training mode first.");
        return false;
    }
}

bool IMLInterface::ClearData()
{
    if (training_mode_ == TRAINING_MODE) {
         DEBUG_PRINTLN("Clearing dataset...");
        dataset_->Clear();
        return true;
    } else {
         DEBUG_PRINTLN("Switch to training mode first.");
        return false;
    }
}

bool IMLInterface::Randomise()
{
    if (training_mode_ == TRAINING_MODE) {
        DEBUG_PRINTLN("Randomising weights...");
        MLRandomise_();
        MLInference_(input_state_);
        return true;
    } else {
        DEBUG_PRINTLN("Switch to training mode first.");
        return false;
    }
}

void IMLInterface::SetIterations(size_t iterations)
{
    n_iterations_ = iterations;
    DEBUG_PRINT("Iterations set to: ");
    DEBUG_PRINTLN(n_iterations_);
}

void IMLInterface::SetZoomEnabled(bool enabled)
{
    zoom_enabled_ = enabled;
    if (disp_) {
        disp_->post(enabled ? "Zoom enabled" : "Zoom disabled");
    }
    if (enabled) {
        zoom_centre_ = input_state_;
        DEBUG_PRINTLN("Zoom centre: ");
        for (auto val : zoom_centre_) {
            DEBUG_PRINT(val);
            DEBUG_PRINT(" ");
        }
        DEBUG_PRINTLN();
    }
}

void IMLInterface::MLSetup_()
{
    // Constants for MLP init
    const unsigned int kBias = 1;
    const std::vector<ACTIVATION_FUNCTIONS> layers_activfuncs = {
        RELU, RELU, RELU, SIGMOID
    };
    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;
    // Layer size definitions
    const std::vector<size_t> layers_nodes = {
        n_inputs_ + kBias,
        10, 10, 14,
        n_outputs_
    };

    // Create dataset
    dataset_ = std::make_unique<Dataset>();
    // Create MLP
    mlp_ = std::make_unique<MLP<float>>(
        layers_nodes,
        layers_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    // State machine
    randomised_state_ = false;
}

void IMLInterface::MLInference_(std::vector<float> input)
{
    if (!dataset_ || !mlp_) {
        DEBUG_PRINTLN("ML not initialized!");
        return;
    }

    if (input.size() != n_inputs_) {
        DEBUG_PRINT("Input size mismatch - ");
        DEBUG_PRINT("Expected: ");
        DEBUG_PRINT(n_inputs_);
        DEBUG_PRINT(", Got: ");
        DEBUG_PRINTLN(input.size());
        return;
    }

    input.push_back(1.0f); // Add bias term
    // Perform inference
    std::vector<float> output(n_outputs_);
    mlp_->GetOutput(input, &output);
    // Process inferenced data
    output_state_ = output;
    SendParamsToQueue(output);
}

void IMLInterface::MLRandomise_()
{
    if (!mlp_) {
        DEBUG_PRINTLN("ML not initialized!");
        return;
    }

    // Randomize weights
    mlp_stored_weights_ = mlp_->GetWeights();
    mlp_->DrawWeights();
    randomised_state_ = true;
}

bool IMLInterface::MLTraining_()
{
    if (!mlp_) {
        DEBUG_PRINTLN("ML not initialized!");
        return false;
    }
    // Restore old weights
    if (randomised_state_) {
        mlp_->SetWeights(mlp_stored_weights_);
    }
    randomised_state_ = false;

    // Prepare for training
    // Extract dataset to training pair
    MLP<float>::training_pair_t dataset(dataset_->GetFeatures(), dataset_->GetLabels());
    // Check and report on dataset size
    DEBUG_PRINT("Feature size ");
    DEBUG_PRINT(dataset.first.size());
    DEBUG_PRINT(", label size ");
    DEBUG_PRINTLN(dataset.second.size());
    if (!dataset.first.size() || !dataset.second.size()) {
        DEBUG_PRINTLN("Empty dataset!");
        return false;
    }
    DEBUG_PRINT("Feature dim ");
    DEBUG_PRINT(dataset.first[0].size());
    DEBUG_PRINT(", label dim ");
    DEBUG_PRINTLN(dataset.second[0].size());
    if (!dataset.first[0].size() || !dataset.second[0].size()) {
        DEBUG_PRINTLN("Empty dataset dimensions!");
        return false;
    }

    // Training loop
    DEBUG_PRINT("Training for max ");
    DEBUG_PRINT(n_iterations_);
    DEBUG_PRINTLN(" iterations...");
    float loss = mlp_->Train(dataset,
            1.,
            n_iterations_,
            0.00001,
            false);
    DEBUG_PRINT("Trained, loss = ");
    DEBUG_PRINTLN(loss, 10);
    return true;
}

void IMLInterface::bindInterface(bool disable_joystick)
{
    // Set up momentary switch callbacks
    MEMLNaut::Instance()->setMomA1Callback([this]() {
        if (this->Randomise() && disp_) {
            disp_->post("Randomised");
        }
    });
    MEMLNaut::Instance()->setMomA2Callback([this]() {
        if (this->ClearData() && disp_) {
            disp_->post("Dataset cleared");
        }
    });

    // Set up toggle switch callbacks
    MEMLNaut::Instance()->setTogA1Callback([this](bool state) {
        if (disp_) {
            disp_->post(state ? "Training mode" : "Inference mode");
        }
        bool trained = this->SetTrainingMode(state ? TRAINING_MODE : INFERENCE_MODE);
        if (disp_ && state == false && trained) {
            disp_->post("Model trained");
        }
    });
    MEMLNaut::Instance()->setTogB2Callback([this](bool state) {
        this->SetZoomEnabled(state);
    });

    MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
        bool saved = this->SaveInput(state ? STORE_VALUE_MODE : STORE_POSITION_MODE);
        if (disp_ && saved) {
            disp_->post(state ? "Where do you want it?" : "Here!");
        }
    });

    if (!disable_joystick) {

        // Set up joystick callbacks
        MEMLNaut::Instance()->setJoyXCallback([this](float value) {
            this->SetInput(0, value);
        });
        MEMLNaut::Instance()->setJoyYCallback([this](float value) {
            this->SetInput(1, value);
        });
        MEMLNaut::Instance()->setJoyZCallback([this](float value) {
            this->SetInput(2, value);
        });
    }

    // Set up other ADC callbacks
    MEMLNaut::Instance()->setRVZ1Callback([this](float value) {
        // Scale value from 0-1 range to 1-3000
        value = 1.0f + (value * 2999.0f);
        const size_t valInt = static_cast<size_t>(value);
        this->SetIterations(valInt);
        if (disp_) {
            disp_->post("Training iterations = " + String(valInt));
        }
    });
    MEMLNaut::Instance()->setRVX1Callback([this](float value) {
        // Scale value from 0-1 range to 0-1
        this->SetZoomFactor(value);
        if (disp_) {
            disp_->post("Zoom factor = " + String(value));
        }
    });

    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        this->ProcessInput();
    });

    MEMLNaut::Instance()->setReSWCallback([this]() {
        if (disp_) {
            disp_->post("Re switch pressed");
        }
    });
}

void IMLInterface::bindUARTInput(std::shared_ptr<UARTInput> uart_input, const std::vector<size_t>& kUARTListenInputs)
{
    if (uart_input) {
        uart_input->SetCallback([this, kUARTListenInputs](size_t sensor_index, float value) {
            // Find param index based on kUARTListenInputs
            auto it = std::find(kUARTListenInputs.begin(), kUARTListenInputs.end(), sensor_index);
            if (it != kUARTListenInputs.end()) {
                size_t param_index = std::distance(kUARTListenInputs.begin(), it);
                DEBUG_PRINTF("Sensor %zu: %f\n", sensor_index, value);
                this->SetInput(param_index, value);
            } else {
                DEBUG_PRINTF("Invalid sensor index: %zu\n", sensor_index);
            }
        });
    }
}

void IMLInterface::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf)
{
    if (midi_interf) {
        midi_interf->SetCCCallback([this](uint8_t cc_number, uint8_t cc_value) {
            DEBUG_PRINTF("MIDI CC %d: %d\n", cc_number, cc_value);
        });
    }
}


std::vector<float> IMLInterface::ZoomCoordinates(const std::vector<float>& coord, const std::vector<float>& zoom_centre, float factor)
{
    // Input validation
    if (coord.size() != zoom_centre.size()) {
        DEBUG_PRINTLN("Warning: ZoomCoordinates - coord and zoom_centre size mismatch");
        return coord;
    }

    if (factor < 0.0f || factor > 1.0f) {
        DEBUG_PRINTLN("Warning: ZoomCoordinates - factor must be between 0 and 1");
        return coord;
    }

    std::vector<float> result;
    result.reserve(coord.size());

    for (size_t i = 0; i < coord.size(); i++) {
        float centre = zoom_centre[i];
        float half_factor = factor * 0.5f;

        // Calculate initial range
        float range_min = centre - half_factor;
        float range_max = centre + half_factor;

        // Adjust centre if range exceeds [0, 1]
        if (range_min < 0.0f) {
            float adjustment = -range_min;
            centre += adjustment;
        } else if (range_max > kMax) {
            float adjustment = range_max - kMax;
            centre -= adjustment;
        }

        // Recalculate adjusted range
        range_min = centre - half_factor;
        range_max = centre + half_factor;

        // Map coordinate from [0, 1] to [range_min, range_max]
        float mapped = range_min + coord[i] * (range_max - range_min);

        // Clamp to [0, 1] as final safety
        mapped = mapped < 0.0f ? 0.0f : (mapped > kMax ? kMax : mapped);

        result.push_back(mapped);
    }

    return result;
}


void IMLInterface::readAnalysisParameters(std::vector<float> params)
{
    // Expecting params size to match n_inputs_
    if (params.size() != n_inputs_) {
        DEBUG_PRINT("Parameter size mismatch - ");
        DEBUG_PRINT("Expected: ");
        DEBUG_PRINT(n_inputs_);
        DEBUG_PRINT(", Got: ");
        DEBUG_PRINTLN(params.size());
        return;
    }

    // Set inputs
    for (size_t i = 0; i < n_inputs_; ++i) {
        SetInput(i, params[i]);
    }
}