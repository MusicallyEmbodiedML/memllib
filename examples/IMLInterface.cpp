#include "IMLInterface.hpp"
#include "../../memlp/Dataset.hpp"
#include "../../memlp/MLP.h"
#include "../hardware/memlnaut/MEMLNaut.hpp"
#include "../interface/UARTInput.hpp"
#include "../interface/MIDIInOut.hpp"
#include "../hardware/memlnaut/display.hpp"
#include <algorithm>

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

    Serial.println("IMLInterface setup done");
    Serial.print("Address of n_inputs_: ");
    Serial.println(reinterpret_cast<uintptr_t>(&n_inputs_));
    Serial.print("Inputs: ");
    Serial.print(n_inputs_);
    Serial.print(", Outputs: ");
    Serial.println(n_outputs_);
}

void IMLInterface::setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp)
{
    setup(n_inputs, n_outputs);
    disp_ = disp;
}

void IMLInterface::SetTrainingMode(training_mode_t training_mode)
{
    Serial.print("Training mode: ");
    Serial.println(training_mode == INFERENCE_MODE ? "Inference" : "Training");

    if (training_mode == INFERENCE_MODE && training_mode_ == TRAINING_MODE) {
        // Train the network!
        MLTraining_();
    }
    training_mode_ = training_mode;
}

void IMLInterface::ProcessInput()
{
    // Check if input is updated
    if (perform_inference_ && input_updated_) {
        Serial.print("Input state: ");
        for (auto val : input_state_) {
            Serial.print(val);
            Serial.print(" ");
        }
        Serial.println();
        MLInference_(input_state_);
        input_updated_ = false;
    }
}

void IMLInterface::SetInput(size_t index, float value)
{
    // Serial.print("Input ");
    // Serial.print(index);
    // Serial.print(" set to: ");
    // Serial.println(value);

    if (index >= n_inputs_) {
        Serial.print("Input index ");
        Serial.print(index);
        Serial.println(" out of bounds.");
        return;
    }

    if (value < 0) {
        value = 0;
    } else if (value > 1.0) {
        value = 1.0;
    }

    // Update state of input
    input_state_[index] = value;
    input_updated_ = true;
}

void IMLInterface::SaveInput(saving_mode_t mode)
{
    if (training_mode_ == TRAINING_MODE) {
        if (STORE_VALUE_MODE == mode) {

            Serial.println("Move input to position...");
            perform_inference_ = false;

        } else {  // STORE_POSITION_MODE

            Serial.println("Creating example in this position.");
            // Save pair in the dataset
            dataset_->Add(input_state_, output_state_);
            perform_inference_ = true;
            MLInference_(input_state_);

        }
    } else {
        Serial.println("Switch to training mode first.");
    }
}

void IMLInterface::ClearData()
{
    if (training_mode_ == TRAINING_MODE) {
        Serial.println("Clearing dataset...");
        dataset_->Clear();
    } else {
        Serial.println("Switch to training mode first.");
    }
}

void IMLInterface::Randomise()
{
    if (training_mode_ == TRAINING_MODE) {
        Serial.println("Randomising weights...");
        MLRandomise_();
        MLInference_(input_state_);
    } else {
        Serial.println("Switch to training mode first.");
    }
}

void IMLInterface::SetIterations(size_t iterations)
{
    n_iterations_ = iterations;
    Serial.print("Iterations set to: ");
    Serial.println(n_iterations_);
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
        Serial.println("ML not initialized!");
        return;
    }

    if (input.size() != n_inputs_) {
        Serial.print("Input size mismatch - ");
        Serial.print("Expected: ");
        Serial.print(n_inputs_);
        Serial.print(", Got: ");
        Serial.println(input.size());
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
        Serial.println("ML not initialized!");
        return;
    }

    // Randomize weights
    mlp_stored_weights_ = mlp_->GetWeights();
    mlp_->DrawWeights();
    randomised_state_ = true;
}

void IMLInterface::MLTraining_()
{
    if (!mlp_) {
        Serial.println("ML not initialized!");
        return;
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
    Serial.print("Feature size ");
    Serial.print(dataset.first.size());
    Serial.print(", label size ");
    Serial.println(dataset.second.size());
    if (!dataset.first.size() || !dataset.second.size()) {
        Serial.println("Empty dataset!");
        return;
    }
    Serial.print("Feature dim ");
    Serial.print(dataset.first[0].size());
    Serial.print(", label dim ");
    Serial.println(dataset.second[0].size());
    if (!dataset.first[0].size() || !dataset.second[0].size()) {
        Serial.println("Empty dataset dimensions!");
        return;
    }

    // Training loop
    Serial.print("Training for max ");
    Serial.print(n_iterations_);
    Serial.println(" iterations...");
    float loss = mlp_->Train(dataset,
            1.,
            n_iterations_,
            0.00001,
            false);
    Serial.print("Trained, loss = ");
    Serial.println(loss, 10);
}

void IMLInterface::bindInterface()
{
    // Set up momentary switch callbacks
    MEMLNaut::Instance()->setMomA1Callback([this]() {
        this->Randomise();
        if (disp_) {
            disp_->post("Randomised");
        }
    });
    MEMLNaut::Instance()->setMomA2Callback([this]() {
        this->ClearData();
        if (disp_) {
            disp_->post("Dataset cleared");
        }
    });

    // Set up toggle switch callbacks
    MEMLNaut::Instance()->setTogA1Callback([this](bool state) {
        if (disp_) {
            disp_->post(state ? "Training mode" : "Inference mode");
        }
        this->SetTrainingMode(state ? TRAINING_MODE : INFERENCE_MODE);
        if (disp_ && state == false) {
            disp_->post("Model trained");
        }
    });
    MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
        this->SaveInput(state ? STORE_VALUE_MODE : STORE_POSITION_MODE);
        if (disp_) {
            disp_->post(state ? "Where do you want it?" : "Here!");
        }
    });

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

    // Set up other ADC callbacks
    MEMLNaut::Instance()->setRVZ1Callback([this](float value) {
        // Scale value from 0-1 range to 1-3000
        value = 1.0f + (value * 2999.0f);
        this->SetIterations(static_cast<size_t>(value));
    });

    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        this->ProcessInput();
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
                Serial.printf("Sensor %zu: %f\n", sensor_index, value);
                this->SetInput(param_index, value);
            } else {
                Serial.printf("Invalid sensor index: %zu\n", sensor_index);
            }
        });
    }
}

void IMLInterface::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf)
{
    if (midi_interf) {
        midi_interf->SetCCCallback([this](uint8_t cc_number, uint8_t cc_value) {
            Serial.printf("MIDI CC %d: %d\n", cc_number, cc_value);
        });
    }
}
