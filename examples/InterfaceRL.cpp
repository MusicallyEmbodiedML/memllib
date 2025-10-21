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
        "Keep going!", "I love you", "In flow", "I believe in you",
        "You're a star!", "Absolutely brilliant!", "This is perfection!",
        "Stunning work!", "You've nailed it!", "Pure genius!",
        "Keep shining!", "Fantastic!", "Incredible vibes!", "Love this journey!"
    };
    String msg = likemsgs[rand() % likemsgs.size()];
    this->storeExperience(1.f * rewardScale);
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_dislike_action() {
    static APP_SRAM std::vector<String> dislikemsgs = {
        "Awful!", "wtf? that sucks", "Get rid of this sound", "Totally shite",
        "I hate this", "Why even bother?", "New sound please!", "No, please no!!!",
        "Thumbs down", "I'm so sorry", "I'm trying my hardest...",
        "I'm doing the best I can", "Please forgive me", "My apologies, learning...",
        "I'll try to do better.", "Didn't mean to disappoint.", "Still figuring things out.",
        "Thanks for the feedback.", "Working on it!", "Oops, my bad.",
        "Learning from this.", "I'll adjust, promise.", "Noted, I'll improve."
    };
    String msg = dislikemsgs[rand() % dislikemsgs.size()];
    this->storeExperience(-1.f * rewardScale);
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_randomiseRL_action() {

    this->randomiseTheActor();
    this->randomiseTheCritic();
    this->generateAction(true);
    DEBUG_PRINTLN("Randomising networks");
    if (msgView) msgView->post("Scrambling Actor and Critic nets");
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
        this->setOptimiseDivisorInterf(value);
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
    return;  // bypassed for now
    this->setRewardScale(value);
    String msg = "Reward scale: " + String(value);
    if (msgView) msgView->post(msg);
}


void InterfaceRL::_randomise_critic_interf()
{
    this->randomiseTheCritic();
    this->generateAction(true);
    DEBUG_PRINTLN("The Critic is confounded");
    if (msgView) msgView->post("Critic: totally confounded");
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


void InterfaceRL::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf)
{
    if (midi_interf) {
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
                    this->_randomise_critic_interf();
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
    controlSize = n_inputs; // control inputs only

    InterfaceBase::setup(controlSize, n_outputs);

    stateSize = controlSize; //state = control inputs + synth params

    actionSize = n_outputs;

    const std::vector<ACTIVATION_FUNCTIONS> actor_activfuncs = {
        RELU, RELU, RELU, TANH
    };

    const std::vector<ACTIVATION_FUNCTIONS> critic_activfuncs = {
        RELU, RELU, RELU, TANH
    };


    actor_layers_nodes = {
        stateSize + bias,
        12, 12, 14,
        actionSize
    };

    critic_layers_nodes = {
        stateSize + actionSize + bias,
        12, 10, 10,
        1
    };

    criticInput.resize(critic_layers_nodes[0]);
    actorControlInput.resize(actor_layers_nodes[0]);
    actorControlInput[actorControlInput.size()-1] = 1.f; // bias

    //init networks
    actor = std::make_shared<MLP<float> > (
        actor_layers_nodes,
        actor_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    actorTarget = std::make_shared<MLP<float> > (
        actor_layers_nodes,
        actor_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    critic = std::make_shared<MLP<float> > (
        critic_layers_nodes,
        critic_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );
    criticTarget = std::make_shared<MLP<float> > (
        critic_layers_nodes,
        critic_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    rewardScale = 1.0f; // Default reward scale

    // Memory limit
    replayMem.setMemoryLimit(memoryLimit);

    ou_noises.reserve(actionSize);
    for(size_t i=0; i < actionSize; i++) {
        ou_noises.push_back(std::make_unique<OrnsteinUhlenbeckNoise>(0.02f, 0.0f, 0.2f, 0.001f, 0.0f));
    }

    // GUI
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

    voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
    MEMLNaut::Instance()->disp->AddView(voiceSpaceSelectView);
    



    rlStatsView = std::make_shared<RLStatsView>("RL Stats");
    MEMLNaut::Instance()->disp->AddView(rlStatsView);
    nnInputsGraphView = std::make_shared<BarGraphView>("NN Inputs", controlSize, 10, TFT_YELLOW, 0.f, 1.f);
    MEMLNaut::Instance()->disp->AddView(nnInputsGraphView);
    nnOutputsGraphView = std::make_shared<BarGraphView>("NN Outputs", n_outputs, 4, TFT_GREEN, 0.f, 1.f);
    MEMLNaut::Instance()->disp->AddView(nnOutputsGraphView);
}


bool InterfaceRL::_save_RL_to_SD(String id) {
    bool success = true;
    // Save actor and actorTarget
    success = success && actor->SaveMLPNetworkSD((FILENAMEROOT + id + String("_actor.bin")).c_str());
    success = success && actorTarget->SaveMLPNetworkSD((FILENAMEROOT + id + String("_actorTarget.bin")).c_str());
    // Save critic and criticTarget
    success = success && critic->SaveMLPNetworkSD((FILENAMEROOT + id + String("_critic.bin")).c_str());
    success = success && criticTarget->SaveMLPNetworkSD((FILENAMEROOT + id + String("_criticTarget.bin")).c_str());
    return success;
}

bool InterfaceRL::_load_RL_from_SD(String id) {
    bool success = true;
    // Load actor and actorTarget
    success = success && actor->LoadMLPNetworkSD((FILENAMEROOT + id + String("_actor.bin")).c_str());
    success = success && actorTarget->LoadMLPNetworkSD((FILENAMEROOT + id + String("_actorTarget.bin")).c_str());
    // Load critic and criticTarget
    success = success && critic->LoadMLPNetworkSD((FILENAMEROOT + id + String("_critic.bin")).c_str());
    success = success && criticTarget->LoadMLPNetworkSD((FILENAMEROOT + id + String("_criticTarget.bin")).c_str());
    return success;
}


void InterfaceRL::optimise() {
    std::vector<trainRLItem> sample = replayMem.sample(batchSize);
    if (sample.size() == batchSize) {
        //run sample through critic target, build training set for critic net
        MLP<float>::training_pair_t ts;
        for(size_t i = 0; i < sample.size(); i++) {
            //---calculate y
            //--calc next-state-action pair
            //get next action from actorTarget given next state
            auto nextStateInput =  sample[i].nextState;
            nextStateInput.push_back(1.f); // bias
            actorTarget->GetOutput(nextStateInput, &actorOutput);

            //use criticTarget to estimate value of next action given next state
            for(size_t j=0; j < stateSize; j++) {
                criticInput[j] = sample[i].nextState[j];
            }
            for(size_t j=0; j < actionSize; j++) {
                criticInput[j+stateSize] = actorOutput[j];
            }
            criticInput[criticInput.size()-1] = 1.f; //bias

            criticTarget->GetOutput(criticInput, &criticOutput);

            //calculate expected reward
            const float y = sample[i].reward + (discountFactor *  criticOutput[0]);
            // std::cout << "[" << i << "]: y: " << y << std::endl;

            //use criticTarget to estimate value of next action given next state
            for(size_t j=0; j < stateSize; j++) {
                criticInput[j] = sample[i].state[j];
            }
            for(size_t j=0; j < actionSize; j++) {
                criticInput[j+stateSize] = sample[i].action[j];
            }
            criticInput[criticInput.size()-1] = 1.f; //bias

            ts.first.push_back(criticInput);
            ts.second.push_back({y});
        }

        //train the critic
        // float loss = critic->Train(ts, criticLearningRate, 1);
        float loss = critic->TrainBatch(ts, criticLearningRateScaled, 1, sample.size(), 0.f, false);

        rlStatsView->setCriticLoss(loss);

        //TODO: size limit to this log
        // criticLossLog.push_back(loss);

        //update the actor

        //for each memory in replay memory sample, and get grads from critic
        std::vector<float> gradientLoss= {1.f};
        float sampleSizeRecr = static_cast<float>(1.f/sample.size());
        std::vector<float> accumulatedGradient(actionSize, 0.0f);

        for(size_t i = 0; i < sample.size(); i++) {
            //use criticTarget to estimate value of next action given next state
            for(size_t j=0; j < stateSize; j++) {
                criticInput[j] = sample[i].state[j];
            }
            auto stateInput = sample[i].state;
            stateInput.push_back(1.f); // bias
            actor->GetOutput(stateInput, &actorOutput);

            // Use this fresh action for critic input
            for(size_t j=0; j < actionSize; j++) {
                criticInput[j+stateSize] = actorOutput[j];
            }

            // for(size_t j=0; j < actionSize; j++) {
            //     criticInput[j+stateSize] = sample[i].action[j];
            // }
            criticInput[criticInput.size()-1] = 1.f; //bias

            critic->CalcGradients(criticInput, gradientLoss); // This calculates dQ/d(input to critic)

            std::vector<float> l0Grads = critic->m_layers[0].GetGrads(); // Gradients of critic's first layer inputs

            // Extract action gradients
            std::vector<float> actionGradients(actionSize);
            for(size_t j = 0; j < actionSize; j++) {
                actionGradients[j] = l0Grads[j+stateSize];
                accumulatedGradient[j] += std::abs(actionGradients[j]);
            }

            // Apply gradients for this specific state
            actor->ApplyPolicyGradient(stateInput, actionGradients, actorLearningRateScaled * sampleSizeRecr);


        }

        float gradNorm = 0.0f;
        for(auto& g : accumulatedGradient) {
            gradNorm += g * g;
        }
        gradNorm = sqrtf(gradNorm);

        // Log gradient statistics
        rlStatsView->setActorGradNorm(gradNorm);


        // soft update the target networks
        criticTarget->SmoothUpdateWeights(critic, smoothingAlpha);
        actorTarget->SmoothUpdateWeights(actor, smoothingAlpha);
    }
}

void InterfaceRL::readAnalysisParameters(std::vector<float> params) {
    //read analysis parameters
    if (params.size() != n_inputs_) {
        DEBUG_PRINTLN("Error: Incorrect number of analysis parameters received.");
        return;
    }
    // Copy params and add bias efficiently
    actorControlInput = params;
    actorControlInput.push_back(1.0f);

    generateAction(true);
}

void InterfaceRL::generateAction(bool donthesitate) {
    if (newInput || donthesitate) {
        newInput = false;
        // std::vector<float> actorOutput; // This was shadowing the member variable

        actorTarget->GetOutput(actorControlInput, &actorOutput); // Use member actorOutput
        for(size_t i=0; i < actorOutput.size(); i++) {
            const float noise = ou_noises[i]->sample();
            actorOutput[i] += noise;
            if (actorOutput[i] < 0.f) {
                actorOutput[i] = -actorOutput[i]; // reflect
            } else if (actorOutput[i] > 1.f) {
                actorOutput[i] = 1.f - fmod(actorOutput[i], 1.f); // reflect at 1.0
            }
        }

        SendParamsToQueue(actorOutput);
        action = actorOutput;
        nnOutputsGraphView->UpdateValues(actorOutput, resetMinMaxFlag);
        resetMinMaxFlag = false;
        nnInputsGraphView->UpdateValues(actorControlInput, false);

        // for(size_t i=0; i < actorOutput.size(); i++) {
        //     const float noise = ou_noise.sample() * knobL; // ou_noise and knobL are not defined here
        //     actorOutput[i] += noise;
        // }
    }
}

void InterfaceRL::storeExperience(float reward) {
    std::vector<float> state = actorControlInput; // actorControlInput already includes bias if setup correctly

    //remove bias if it's part of actorControlInput for storage
    // The actorControlInput is stateSize + bias.
    // If 'state' in trainRLItem should not have bias, it needs to be removed.
    // The original code pops_back, assuming bias is the last element.
    if (!state.empty() && state.size() == stateSize + bias) { // Check if bias is likely present
       state.pop_back(); // remove bias if it was added for network input
    }


    // for(size_t i=0; i < state.size(); i++) { // state here is without bias
    //     DEBUG_PRINTF("%f\t", state[i]);
    // }
    // DEBUG_PRINTLN();
    // nextState in DDPG is the state resulting from taking 'action' in 'state'.
    // Here, 'state' is used as 'nextState', which is common if the environment is static
    // or if the 'nextState' is the same as the current state for the purpose of this reward.
    // However, typically nextState would be s_t+1.
    // If actorControlInput is s_t+1, then the current state s_t needs to be stored from a previous step.
    // For now, assuming state is s_t and nextState is also s_t for this specific implementation detail.
    trainRLItem trainItem = {state, action, reward, state}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
    replayMem.add(trainItem, millis());
}
