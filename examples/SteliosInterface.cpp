#include "SteliosInterface.hpp"
#include "../../memlp/Dataset.hpp"
#include "../../memlp/MLP.h"
#include "../hardware/memlnaut/MEMLNaut.hpp"
#include "../interface/UARTInput.hpp"
#include "../interface/MIDIInOut.hpp"
#include "../hardware/memlnaut/display.hpp"
#include <algorithm>
#include <memory>


void SteliosInterface::setup(size_t n_inputs, size_t n_outputs)
{
    InterfaceBase::setup(n_inputs, n_outputs);
    // Additional setup code specific to SteliosInterface
    n_inputs_ = n_inputs;
    n_outputs_ = n_outputs;

    MLSetup_();
    n_iterations_ = 1000;
    input_state_.resize(n_inputs, 0.5f);
    output_state_.resize(n_outputs, 0);
    // Init/reset state machine
    perform_inference_ = true;
    input_updated_ = false;

    zoom_centre_ = std::vector<float>(n_inputs_, 0.5f);
    zoom_enabled_ = false;
    zoom_factor_ = 0.5f;

    selected_category_ = 0;
    class_occurrences_.resize(n_outputs_, 0);

    uart_output = std::make_shared<SerialUSBOutput>();

    // Configure SD card
#if 0
    auto sdcard = std::make_unique<SDCard>();
    sdcard->SetCardEventCallback([this](bool inserted) {
        if (inserted) {
            DEBUG_PRINTLN("SD card inserted.");
            if (disp_) {
                disp_->post("SD card inserted");
            }
        } else {
            DEBUG_PRINTLN("SD card not present.");
            if (disp_) {
                disp_->post("SD card absent");
            }
        }
    });
    sdcard->Poll(); // Initial poll to check card status
#endif

    DEBUG_PRINTLN("SteliosInterface setup done");
    DEBUG_PRINT("Address of n_inputs_: ");
    DEBUG_PRINTLN(reinterpret_cast<uintptr_t>(&n_inputs_));
    DEBUG_PRINT("Inputs: ");
    DEBUG_PRINT(n_inputs_);
    DEBUG_PRINT(", Outputs: ");
    DEBUG_PRINTLN(n_outputs_);
}

void SteliosInterface::setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp)
{
    disp_ = disp;
    if (disp_) {
        disp_->statusPost("No class sel", 0);
        disp_->statusPost("No class out", 1);
    }
    setup(n_inputs, n_outputs);
}

void SteliosInterface::ProcessInput()
{
    // Check if input is updated
    if (perform_inference_ && input_updated_) {
        //DEBUG_PRINT("Input state: ");
        // for (auto val : input_state_) {
        //     DEBUG_PRINT(val);
        //     DEBUG_PRINT(" ");
        // }
        DEBUG_PRINTLN();
        //disp_->post("In:" + String(input_state_[0]) + " " + String(input_state_[1]) + " " + String(input_state_[2]));
        // Only zoom here
        std::vector<float> zoomed_input = input_state_;
        // TODO AM maybe re-enable zoom
        // if (zoom_enabled_) {
        //     zoomed_input = ZoomCoordinates(input_state_, zoom_centre_, zoom_factor_);
        //     // DEBUG_PRINT("Zoomed input: ");
        //     // for (auto val : zoomed_input) {
        //     //     DEBUG_PRINT(val);
        //     //     DEBUG_PRINT(" ");
        //     // }
        //      DEBUG_PRINTLN();
        // }
        MLInference_(zoomed_input);
        input_updated_ = false;
    }
}

void SteliosInterface::SetInput(size_t index, float value)
{
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

bool SteliosInterface::SaveInput(std::vector<float> category)
{
    WRITE_VOLATILE(perform_inference_, false); // Stop inference while saving
    // Save pair in the dataset
    dataset_->Add(input_state_, category);
    DEBUG_PRINT("Saved input.");

    WRITE_VOLATILE(perform_inference_, true); // Resume inference
    return true;
}

bool SteliosInterface::Train()
{
    WRITE_VOLATILE(perform_inference_, false); // Stop inference while training
    DEBUG_PRINTLN("Training MLP...");
    bool trained = MLTraining_();
    //DEBUG_PRINT("Training done, loss = ");
    //DEBUG_PRINTLN(mlp_->GetLoss(), 10);
    WRITE_VOLATILE(perform_inference_, true); // Resume inference
    return trained;
}

bool SteliosInterface::ClearData()
{
    WRITE_VOLATILE(perform_inference_, false); // Stop inference while clearing
    DEBUG_PRINTLN("Clearing dataset...");
    dataset_->Clear();
    class_occurrences_.assign(n_outputs_, 0);
    WRITE_VOLATILE(perform_inference_, true); // Resume inference
    return true;
}

bool SteliosInterface::Randomise()
{
    WRITE_VOLATILE(perform_inference_, false); // Stop inference while randomising
    DEBUG_PRINTLN("Randomising weights...");
    MLRandomise_();
    MLInference_(input_state_);
    WRITE_VOLATILE(perform_inference_, true); // Resume inference
    return true;
}

void SteliosInterface::SetIterations(size_t iterations)
{
    n_iterations_ = iterations;
    DEBUG_PRINT("Iterations set to: ");
    DEBUG_PRINTLN(n_iterations_);
}

void SteliosInterface::SetZoomEnabled(bool enabled)
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

void SteliosInterface::MLSetup_()
{
    // Constants for MLP init
    const unsigned int kBias = 1;
    const std::vector<ACTIVATION_FUNCTIONS> layers_activfuncs {
        RELU, RELU, RELU, LINEAR
    };
    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;
    // Layer size definitions
    const std::vector<size_t> layers_nodes = {
        n_inputs_ + kBias,
        20, 20, 10,
        n_outputs_
    };

    // Create dataset
    dataset_ = std::make_unique<Dataset>();
    // Create MLP
    mlp_ = std::make_unique<MLP<float>>(
        layers_nodes,
        layers_activfuncs,
        loss::LOSS_CATEGORICAL_CROSSENTROPY,
        use_constant_weight_init,
        constant_weight_init
    );
    // Load weights from file if available
    if (mlp_->LoadMLPNetwork(kFilename)) {
        DEBUG_PRINTLN("Loaded MLP weights from file.");
        if (disp_) {
            disp_->post("Weights loaded from SD");
        }
    } else {
        DEBUG_PRINTLN("No MLP weights file found, using default initialization.");
        if (disp_) {
            disp_->post("No SD - random init!");
        }
    }

    // State machine
    randomised_state_ = false;
}

void SteliosInterface::MLInference_(std::vector<float> input)
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
    uart_output->SendFloatArray(output);

    if (disp_) {
        // Find the class with highest output (argmax)
        size_t class_id = 0;
        float max_output = output_state_[0];
        for (size_t i = 1; i < output_state_.size(); ++i) {
            if (output_state_[i] > max_output) {
                max_output = output_state_[i];
                class_id = i;
            }
        }

        // Calculate confidence as the maximum probability
        float confidence = max_output;

        // Format buffer as Out:<class_id> (<confidence>)
        String buf = "Out:" + String(class_id) + " (" + String(confidence, 3) + ")";
        disp_->statusPost(buf, 1);
    }
}

void SteliosInterface::MLRandomise_()
{
    if (!mlp_) {
        DEBUG_PRINTLN("ML not initialized!");
        return;
    }

    // Randomize weights
    //mlp_stored_weights_ = mlp_->GetWeights();
    mlp_->DrawWeights();
    //randomised_state_ = true;
}

bool SteliosInterface::MLTraining_()
{
    if (!mlp_) {
        DEBUG_PRINTLN("ML not initialized!");
        return false;
    }
    // Restore old weights
    // if (randomised_state_) {
    //     mlp_->SetWeights(mlp_stored_weights_);
    // }
    // randomised_state_ = false;

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
            0.5f,
            n_iterations_,
            0.00001,
            false);
    disp_->post("Trained, loss = " + String(loss, 5));

    // Save the trained model to SD card
    if (mlp_->SaveMLPNetwork(kFilename)) {
        DEBUG_PRINTLN("Saved MLP weights to file.");
        if (disp_) {
            disp_->post("Weights saved to SD");
        }
    } else {
        DEBUG_PRINTLN("Failed to save MLP weights to file.");
        if (disp_) {
            disp_->post("Failed to save weights");
        }
    }
    return true;
}

void SteliosInterface::bindInterface()
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
    MEMLNaut::Instance()->setMomB1Callback([this]() {
        // Trigger training with current class
        // - Convert selected_category_ to one-hot vector
        std::vector<float> class_vector(this->n_outputs_, 0.0f);
        if (this->selected_category_ < this->n_outputs_) {
            class_vector[this->selected_category_] = 1.0f;
        }
        // Increment class occurrence count
        if (this->selected_category_ < this->class_occurrences_.size()) {
            this->class_occurrences_[this->selected_category_] += 1;
        }
        // Save input
        if (this->SaveInput(class_vector) && disp_) {
            String buf = "";
            for (size_t i = 0; i < this->class_occurrences_.size(); ++i) {
                buf += "Cl" + String(i) + ":" + String(this->class_occurrences_[i]) + " ";
            }
            disp_->post(buf);
        }
    });
    MEMLNaut::Instance()->setMomB2Callback([this]() {
        if (disp_) {
            disp_->post("Training...");
        }
        if (!this->Train() && disp_) {
            disp_->post("Failed to train");
        }
    });

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
    MEMLNaut::Instance()->setRVY1Callback([this](float value) {
        // Map [0..1] to a category index
        size_t category_index = static_cast<size_t>(value * (this->n_outputs_));
        if (category_index >= this->n_outputs_) {
            category_index = this->n_outputs_ - 1; // Clamp to max index
        }
        if (category_index != this->selected_category_) {
            this->selected_category_ = category_index;
            if (disp_) {
                disp_->statusPost("Class: " + String(category_index), 0);
            }
        }
    });

    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        this->ProcessInput();
    });
}

void SteliosInterface::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf)
{
    if (midi_interf) {
        midi_interf->SetCCCallback([this](uint8_t cc_number, uint8_t cc_value) {
            DEBUG_PRINTF("MIDI CC %d: %d\n", cc_number, cc_value);
        });
    }
}


std::vector<float> SteliosInterface::ZoomCoordinates(const std::vector<float>& coord, const std::vector<float>& zoom_centre, float factor)
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
