#include "InterfaceRL.hpp"
#include "../utils/sharedMem.hpp" // Required for READ_VOLATILE, sharedMem constants and PERIODIC_DEBUG
#include <Arduino.h>     // Required for Serial, millis, delay
#include "../hardware/memlnaut/MEMLNaut.hpp" // Required for MEMLNaut::Instance()
// display.hpp is included via InterfaceRL.hpp


static const String FILENAMEROOT("mlp_rl_");


// Protected helper method implementations
void InterfaceRL::_perform_like_action() {
    static APP_SRAM std::vector<String> likemsgs = {
        "Wow, incredible", "Awesome", "That's amazing", "Unbelievable+",
        "I love it!!", "More of this", "Yes!!!!", "A-M-A-Z-I-N-G",
        "Keep going!", "In flow", "I believe in you",
        "Absolutely brilliant!", "This is perfection!",
        "Stunning work!", "Pure genius!",
        "Keep shining!", "Fantastic!", "Incredible vibes!", "Love this journey!",
        "Super cool!"
    };
    String msg = likemsgs[rand() % likemsgs.size()];
    this->storeExperience(1.f, controlInput, action);
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_dislike_action() {
    static APP_SRAM std::vector<String> dislikemsgs = {
        "oh no!", "Get rid of this sound", 
        "Why even bother?", "New sound please!", "No, please no!!!",
        "Thumbs down", "I'm so sorry", "I'm trying my hardest...",
        "I'm doing the best I can", "Learning...",
	    "I'll try to do better.", "Still figuring things out.",
        "Thanks for the feedback.", "Working on it!", "Oops, my bad.",
        "Learning from this.", "I'll adjust, promise.", "Noted", "Rearranging",
        "Let's move on!"
    };
    String msg = dislikemsgs[rand() % dislikemsgs.size()];
    this->storeExperience(-1.f, controlInput, action);
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_randomiseRL_action() {

    this->randomiseTheNetwork();
    this->generateAction(true);
    DEBUG_PRINTLN("Randomising networks");
    if (msgView) msgView->post("Scrambling the network");
}

// Public trigger methods (updated to call protected helpers)
void InterfaceRL::trigger_like() {
    DEBUG_PRINTLN("Like!");
    _perform_like_action();
    DEBUG_PRINTLN("Liked!");
}

void InterfaceRL::trigger_dislike() {
    DEBUG_PRINTLN("Dislike!");
    _perform_dislike_action();
    DEBUG_PRINTLN("Disliked!");
}

// void InterfaceRL::trigger_randomiseRL() {
//     _perform_randomiseRL_action();
// }


void InterfaceRL::setOptimiseDivisorInterf(float value)
{
    size_t divisor = 1 + (value * 100);
    String msg;
    if (divisor > 90) {
        divisor = 999999;
        msg = "Optimisation paused";
    }else{
        msg = "Optimise every " + String(divisor) + " cycles";
    }
    if (msgView) msgView->post(msg);
    this->setOptimiseDivisor(divisor);
    DEBUG_PRINTLN(msg);
}


void InterfaceRL::bind_RL_interface(bool disable_joystick) {

    // Set up momentary switch callbacks
    MEMLNaut::Instance()->setMomA1Callback([this]() { // scr_ref no longer captured directly
        if (MEMLNaut::Instance()->getMOMA1State()) {
            this->trigger_like();
        }
    });
    MEMLNaut::Instance()->setMomA2Callback([this]() { // scr_ref no longer captured directly
        if (MEMLNaut::Instance()->getMOMA2State()) {
            this->trigger_dislike();
        }
    });
    MEMLNaut::Instance()->setMomB1Callback([this]() { // scr_ref no longer captured directly
        if (MEMLNaut::Instance()->getMOMB1State()) {
            // this->trigger_randomiseRL();
            _perform_randomiseRL_action();            
        }
    });
    MEMLNaut::Instance()->setMomB2Callback([this]() { // scr_ref no longer captured directly
        if (MEMLNaut::Instance()->getMOMB2State()) {
            joltNetworks();
        }
    });


    if (!disable_joystick) {
        MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
            //button down
            if (state) {
                //store output
                savedAction = action;
                actionBeingDragged=true;
            }else{
                //button up
                this->storeExperience(1.f, controlInput, savedAction);
                actionBeingDragged=false;
            }
            msgView->post(state ? "Where do you want it?" : "Here!");
        });
        // Set up ADC callbacks
        MEMLNaut::Instance()->setJoyXCallback([this](float value) {
            this->setState(0, value);
        });
        MEMLNaut::Instance()->setJoyYCallback([this](float value) {
            this->setState(1, value);
        });
        MEMLNaut::Instance()->setJoyZCallback([this](float value) {
            this->setState(2, value);
        });
    }


    MEMLNaut::Instance()->setTogB1Callback([this](bool state) { // scr_ref no longer captured directly
        if (state) {
            this->_forget_replay_mem_interf();
        }
    });


    MEMLNaut::Instance()->setRVX1Callback([this](float value) { // scr_ref no longer captured directly
        this->setRewardScaleInterf(value);
    });

    MEMLNaut::Instance()->setRVY1Callback([this](float value) {
        // this->setRewardScaleInterf(value);
        this->setLRScale(value);
    });

    MEMLNaut::Instance()->setRVZ1Callback([this](float value) { // scr_ref no longer captured directly
        // value *= 0.1f; // Scale down the value
        // this->setLearningRate(value);
        // String msg = "LR: " + String(value,8);
        // if (msgView) msgView->post(msg);
        // this->setDiscountFactor(value);
        // String msg = "Discount factor: " + String(value);
        // if (msgView) msgView->post(msg);
        setNoiseLevel(value);
    });
    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        this->optimiseSometimes();
        this->generateAction();

    });
}


void InterfaceRL::setRewardScaleInterf(float value)
{
    this->setRewardScale(value);
    String msg = "Reward scale: " + String(value);
    if (msgView) msgView->post(msg);
}



void InterfaceRL::_forget_replay_mem_interf()
{
    this->forgetMemory();
    static APP_SRAM std::vector<String> forgetmsgs = {
        "Erasing my memory", "Forgetting everything", "Memory wiped","Thank you Susan?",
        "Starting afresh", "Why care about the past?","Living in the moment"
    };
    String msg = forgetmsgs[rand() % forgetmsgs.size()];

    if (msgView) msgView->post(msg);
}


void InterfaceRL::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf, bool enableFootcontroller)
{
    if (midi_interf && enableFootcontroller) {
        midi_interf->SetCCCallback([this] (uint8_t cc_number, uint8_t cc_value) {
            Serial.printf("MIDI CC %d: %d\n", cc_number, cc_value);
            switch(cc_number) {
                case 1:
                {
                    this->_perform_like_action();
                    break;
                }
                case 2:
                {
                    this->_perform_dislike_action();
                    break;
                }
                case 3:
                {
                    this->_perform_randomiseRL_action();
                    break;
                }
                case 4:
                {
                    this->_forget_replay_mem_interf();
                    break;
                }
                case 5:
                {
                    if (midi5cb) {
                        midi5cb(cc_value);

                    }else{
                        static constexpr float cc_scale = 1.f/(127.f-20.f);
                        // Less than 20 on cc_value is considered 0
                        // scale [20..127] to [0.0, 1.0]
                        if (cc_value < 20) {
                            cc_value = 0;
                        } else {
                            cc_value -= 20; // Shift range to [0, 107]
                        }
                        float scale = static_cast<float>(cc_value) * cc_scale;
                        //this->setRewardScaleInterf(scale);
                        this->setNoiseLevel(scale);
                    }
                    break;
                }
                case 6:
                {
                    if (midi6cb) {
                        midi6cb(cc_value);

                    }else{
                        static constexpr float cc_scale = 1.f/(127.f-20.f);
                        // Less than 20 on cc_value is considered 0
                        // scale [20..127] to [0.0, 1.0]
                        if (cc_value < 20) {
                            cc_value = 0;
                        } else {
                            cc_value -= 20; // Shift range to [0, 107]
                        }
                        float opt = static_cast<float>(cc_value) * cc_scale;
                        this->setOptimiseDivisorInterf(1.f - opt);
                    }
                    break;
                }
            };
        });
    }

    midi_ = midi_interf;
}

void InterfaceRL::setup(size_t n_inputs, size_t n_outputs)
{

    InterfaceBase::setup(n_inputs, n_outputs);

    const std::vector<ACTIVATION_FUNCTIONS> activfuncs = {
        RELU, RELU, HARDSIGMOID
    };



    std::vector<size_t> layers_nodes = {
        n_inputs,
        16, 16,
        n_outputs
    };

    controlInput.resize(layers_nodes[0]);

    //init networks
    synthMapping = std::make_shared<MLP<float> > (
        layers_nodes,
        activfuncs,
        loss::LOSS_MSE,
        0,
        0
    );

    // synthMapping->InitXavier();
    synthMapping->RandomiseWeightsAndBiasesLin(-0.9f,0.9f, 0, 0.5);

    rewardScale = 1.0f; // Default reward scale

    // randomiseTheNetwork();

    // Memory limit
    replayMem.setMemoryLimit(memoryLimit);

    ou_noises.reserve(n_outputs);
    for(size_t i=0; i < n_outputs; i++) {
        ou_noises.push_back(std::make_unique<OrnsteinUhlenbeckNoise>(0.02f, 0.0f, 0.2f, 0.001f, 0.0f));
    }

    itemsToRemove.reserve(replayMem.getMemoryLimit());

    // GUI
    nnOutputsGraphView = std::make_shared<BarGraphView>("NN Outputs", n_outputs, 4, TFT_GREEN, 0.f, 1.f);
    MEMLNaut::Instance()->disp->AddView(nnOutputsGraphView);
    rlStatsView = std::make_shared<RLStatsView>("RL Stats");
    MEMLNaut::Instance()->disp->AddView(rlStatsView);
    nnInputsGraphView = std::make_shared<BarGraphView>("NN Inputs", n_inputs, 10, TFT_YELLOW, 0.f, 1.f);
    MEMLNaut::Instance()->disp->AddView(nnInputsGraphView);


    msgView = std::make_shared<MessageView>("Messages");
    MEMLNaut::Instance()->disp->AddView(msgView);

    fileSaveView = std::make_shared<BlockSelectView>("Save Model", TFT_BLUE);
    fileSaveView->SetOnSelectCallback([this] (size_t id) {
        fileSaveView->SetMessage("Saving model " + String(id));
        if (MEMLNaut::Instance()->startSD()) {
            if (this->_save_RL_to_SD(String(id))) {
                fileSaveView->SetMessage("Model saved successfully to slot " + String(id));
            } else {
                fileSaveView->SetMessage("Failed to save model");
            }
            MEMLNaut::Instance()->stopSD();
        } else {
            fileSaveView->SetMessage("SD card error - is it inserted and formatted?");
        }
    });
    MEMLNaut::Instance()->disp->AddView(fileSaveView);

    fileLoadView = std::make_shared<BlockSelectView>("Load Model", TFT_PURPLE);
    fileLoadView->SetOnSelectCallback([this] (size_t id) {
        fileLoadView->SetMessage("Loading model " + String(id));
        if (MEMLNaut::Instance()->startSD()) {
            if (this->_load_RL_from_SD(String(id))) {
                fileLoadView->SetMessage("Model loaded successfully ");
            } else {
                fileLoadView->SetMessage("Failed to load model");
            }
            MEMLNaut::Instance()->stopSD();
        } else {
            fileLoadView->SetMessage("SD card error - is it inserted and formatted?");
        }

    });
    MEMLNaut::Instance()->disp->AddView(fileLoadView);

    



}


bool InterfaceRL::_save_RL_to_SD(String id) {
    bool success = true;
    // Save actor and actorTarget
    success = success && synthMapping->SaveMLPNetworkSD((FILENAMEROOT + id + String("_net.bin")).c_str());
    return success;
}

bool InterfaceRL::_load_RL_from_SD(String id) {
    bool success = true;
    // Load actor and actorTarget
    success = success && synthMapping->LoadMLPNetworkSD((FILENAMEROOT + id + String("_net.bin")).c_str());
    return success;
}




void InterfaceRL::optimise() {
    //std::vector<trainStatelessRLItem> sample = replayMem.sample(batchSize);
    std::vector<size_t> sample = replayMem.sampleIndices(batchSize);
    if (sample.size() >2) {
        //run sample through network
        size_t batchSizePos=0;
        size_t batchSizeNeg=0;
        float avgRewardPos=0.f;
        float avgRewardNeg=0.f;
        MLP<float>::training_pair_t tsPositive, tsNegative;

        // Pre-allocate to avoid repeated allocations
        tsPositive.first.reserve(sample.size());
        tsPositive.second.reserve(sample.size());
        tsNegative.first.reserve(sample.size());
        tsNegative.second.reserve(sample.size());


        for(auto &i: sample) {
		//for(size_t i = 0; i < sample.size(); i++) {

            // Validate input and action sizes
            // if (sample[i].input.size() != n_inputs_ || sample[i].action.size() != n_outputs_) {
            //     Serial.printf("ERROR: Invalid sample %d - input_size=%d (expected %d), action_size=%d (expected %d)\n",
            //         i, sample[i].input.size(), n_inputs_, sample[i].action.size(), n_outputs_);
            //     continue;
            // }

            if (replayMem.getItem(i).reward > 0) {
                tsPositive.first.push_back(replayMem.getItem(i).input);
                tsPositive.second.push_back(replayMem.getItem(i).action);
                batchSizePos++;
                avgRewardPos+=replayMem.getItem(i).reward;
            }else{
                tsNegative.first.push_back(replayMem.getItem(i).input);
                tsNegative.second.push_back(replayMem.getItem(i).action);
                batchSizeNeg++;
                float reward = replayMem.getItem(i).reward;
                avgRewardNeg+=reward;
                //decay towards 0
                const float rewardCom = 1.f+ reward;
                reward = reward + (0.005 * rewardCom);
				replayMem.getItem(i).reward = reward; 
                Serial.printf("neg rw %d %f\n", i, reward);
                if (reward > -0.01) {
                    itemsToRemove.push_back(i);
                    Serial.printf("Erasing neg exp %d\n",i);
                }
            }

        }
        float lossPositive{0.f};
        float lossNegative{0.f};
        if (batchSizePos > 0){
            avgRewardPos /= static_cast<float>(batchSizePos);
            // Serial.printf("Training %d positive samples, avg reward: %f\n", batchSizePos, avgRewardPos);
            lossPositive = synthMapping->TrainBatch(tsPositive, learningRateScaled * avgRewardPos, 1, batchSize, 0.f, false);
        }
        if (batchSizeNeg > 0){
            avgRewardNeg /= static_cast<float>(batchSizeNeg);
            // Serial.printf("Training %d negative samples, avg reward: %f\n", batchSizeNeg, avgRewardNeg);
            lossNegative = synthMapping->TrainBatch(tsNegative, learningRateScaled * 0.1 * avgRewardNeg, 1, batchSize, 0.f, false);
        }

        if (replayMem.removeItems(itemsToRemove)) {
            itemsToRemove.clear();
        };

        rlStatsView->setLoss(lossPositive);

    }
}

void InterfaceRL::readAnalysisParameters(std::vector<float> params) {
    //read analysis parameters
    if (params.size() != n_inputs_) {
        DEBUG_PRINTLN("Error: Incorrect number of analysis parameters received.");
        return;
    }
    controlInput = params;

    generateAction(true);
}

void InterfaceRL::generateAction(bool donthesitate) {
    if (newInput || donthesitate) {
        newInput = false;

        if (!actionBeingDragged) {
            synthMapping->GetOutput(controlInput, &mappingOutput); 
            for(size_t i=0; i < mappingOutput.size(); i++) {
                const float noise = ou_noises[i]->sample();
                mappingOutput[i] += noise;
                if (mappingOutput[i] < 0.f) {
                    mappingOutput[i] = fmod(-mappingOutput[i],1.f); // reflect
                } else if (mappingOutput[i] > 1.f) {
                    mappingOutput[i] = 1.f - fmod(mappingOutput[i], 1.f); // reflect at 1.0
                }
            }
        }
        SendParamsToQueue(mappingOutput);
        action = mappingOutput;
        nnOutputsGraphView->UpdateValues(mappingOutput, resetMinMaxFlag);
        resetMinMaxFlag = false;
        nnInputsGraphView->UpdateValues(controlInput, false);
    }
}

// void InterfaceRL::storeExperience(float reward) {
//     std::vector<float> state = controlInput; 
//     trainStatelessRLItem trainItem = {state, action, reward}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
//     replayMem.add(trainItem, millis());
// }

void InterfaceRL::storeExperience(float reward, std::vector<float> &experienceState, std::vector<float> &experienceAction ) {
    trainStatelessRLItem trainItem = {experienceState, experienceAction, reward * rewardScale}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
    replayMem.add(trainItem, millis());
}
