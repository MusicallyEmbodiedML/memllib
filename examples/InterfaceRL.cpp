#include "InterfaceRL.hpp"
#include "../utils/sharedMem.hpp" // Required for READ_VOLATILE, sharedMem constants and PERIODIC_DEBUG
#include <Arduino.h>     // Required for Serial, millis, delay
#include "../hardware/memlnaut/MEMLNaut.hpp" // Required for MEMLNaut::Instance()
// display.hpp is included via InterfaceRL.hpp


// Protected helper method implementations
void InterfaceRL::_perform_like_action() {
    if (!m_scr_ptr) return; // Guard against null pointer
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
    m_scr_ptr->post(msg);
}

void InterfaceRL::_perform_dislike_action() {
    if (!m_scr_ptr) return; // Guard against null pointer
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
    m_scr_ptr->post(msg);
}

void InterfaceRL::_perform_randomiseRL_action() {
    if (!m_scr_ptr) return; // Guard against null pointer
    this->randomiseTheActor();
    this->generateAction(true);
    DEBUG_PRINTLN("The Actor is confused");
    m_scr_ptr->post("Actor: i'm confused");
}

// Public trigger methods (updated to call protected helpers)
void InterfaceRL::trigger_like() {
    _perform_like_action();
}

void InterfaceRL::trigger_dislike() {
    _perform_dislike_action();
}

void InterfaceRL::trigger_randomiseRL() {
    _perform_randomiseRL_action();
}

void InterfaceRL::bind_RL_interface(display& scr_ref, bool disable_joystick) {
    m_scr_ptr = &scr_ref; // Store the pointer to the display object

    // Set up momentary switch callbacks
    MEMLNaut::Instance()->setMomA1Callback([this]() { // scr_ref no longer captured directly
        this->trigger_like();
    });
    MEMLNaut::Instance()->setMomA2Callback([this]() { // scr_ref no longer captured directly
        this->trigger_dislike();
    });
    MEMLNaut::Instance()->setMomB1Callback([this]() { // scr_ref no longer captured directly
        this->trigger_randomiseRL();
    });
    MEMLNaut::Instance()->setMomB2Callback([this]() { // scr_ref no longer captured directly
        this->randomiseTheCritic();
        this->generateAction(true);
        DEBUG_PRINTLN("The Critic is confounded");
        if (m_scr_ptr) m_scr_ptr->post("Critic: totally confounded");
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
            this->forgetMemory();
            static APP_SRAM std::vector<String> forgetmsgs = {
                "Erasing my memory", "Forgetting everything", "Memory wiped","Thank you Susan?",
                "Starting afresh", "Why care about the past?","Living in the moment"
            };
            String msg = forgetmsgs[rand() % forgetmsgs.size()];

            if (m_scr_ptr) m_scr_ptr->post(msg);

        }
    });


    MEMLNaut::Instance()->setRVX1Callback([this](float value) { // scr_ref no longer captured directly
        size_t divisor = 1 + (value * 100);
        String msg;
        if (divisor > 90) {
            divisor = 999999;
            msg = "Optimisation paused";
        }else{
            msg = "Optimise every " + String(divisor) + " cycles";
        }
        if (m_scr_ptr) m_scr_ptr->post(msg);
        this->setOptimiseDivisor(divisor);
        DEBUG_PRINTLN(msg);
    });

    MEMLNaut::Instance()->setRVY1Callback([this](float value) { // scr_ref no longer captured directly
        this->setRewardScale(value);
        String msg = "Reward scale: " + String(value);
        if (m_scr_ptr) m_scr_ptr->post(msg);
    });
    MEMLNaut::Instance()->setRVZ1Callback([this](float value) { // scr_ref no longer captured directly
        // value *= 0.1f; // Scale down the value
        // this->setLearningRate(value);
        // String msg = "LR: " + String(value,8);
        // if (m_scr_ptr) m_scr_ptr->post(msg);
        // this->setDiscountFactor(value);
        // String msg = "Discount factor: " + String(value);
        // if (m_scr_ptr) m_scr_ptr->post(msg);
        setNoiseLevel(value);
    });
    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        this->optimiseSometimes();
        this->generateAction();
    });
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
                    break;
                }
            };
        });
    }
}

void InterfaceRL::setup(size_t n_inputs, size_t n_outputs)
{
    controlSize = n_inputs; // control inputs only

    InterfaceBase::setup(controlSize, n_outputs);

    stateSize = controlSize; //state = control inputs + synth params

    actionSize = n_outputs;

    actor_layers_nodes = {
        stateSize + bias,
        10, 12,
        actionSize
    };

    critic_layers_nodes = {
        stateSize + actionSize + bias,
        10, 10,
        1
    };

    criticInput.resize(critic_layers_nodes[0]);
    actorControlInput.resize(actor_layers_nodes[0]);
    actorControlInput[actorControlInput.size()-1] = 1.f; // bias

    //init networks
    actor = std::make_shared<MLP<float> > (
        actor_layers_nodes,
        layers_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    actorTarget = std::make_shared<MLP<float> > (
        actor_layers_nodes,
        layers_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );

    critic = std::make_shared<MLP<float> > (
        critic_layers_nodes,
        layers_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );
    criticTarget = std::make_shared<MLP<float> > (
        critic_layers_nodes,
        layers_activfuncs,
        loss::LOSS_MSE,
        use_constant_weight_init,
        constant_weight_init
    );
}

void InterfaceRL::setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp) {
    this->setup(n_inputs, n_outputs);
    m_scr_ptr = disp.get(); // Store the pointer to the display object
}

void InterfaceRL::optimise() {
    constexpr size_t batchSize = 8;
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
        float loss = critic->Train(ts, criticLearningRate, 1);

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

            for(size_t j=0; j < actionSize; j++) {
                accumulatedGradient[j] += l0Grads[j+stateSize];
            }


            // for(size_t j=0; j < actionSize; j++) {
            //     actorGradient[j] += l0Grads[j+stateSize] * sampleSizeRecr; // Accumulate gradients for actions
            //     totalActorGradient += actorGradient[j];      // Sum of gradients
            // }

            // actor->ApplyLoss(stateInput, actorGradient, actorLearningRate);

            // float avgQ = criticOutput[0];
            // float gradMagnitude = 0;
            // for(auto& g : actorGradient) gradMagnitude += g*g;
            // gradMagnitude = sqrt(gradMagnitude);

            //  DEBUG_PRINTF("Q-value: %.3f, Grad magnitude: %.3f\n", avgQ, gradMagnitude);



        }

        for(size_t j=0; j < actionSize; j++) {
            accumulatedGradient[j] *= sampleSizeRecr;
        }

        // Simple gradient clipping
        const float maxGradNorm = 0.5f;  // Start conservative
        float gradNorm = 0.0f;

        for(auto& g : accumulatedGradient) {
            gradNorm += g * g;
        }
        gradNorm = sqrt(gradNorm);

        if (gradNorm > maxGradNorm) {
            float scale = maxGradNorm / gradNorm;
            for(auto& g : accumulatedGradient) {
                g *= scale;
            }

            // Optional: notify user of instability
            if (gradNorm > maxGradNorm * 2.0f) {
                m_scr_ptr->post("Learning unstable - adjusting...");
            }
        }

        std::vector<float> avgState(stateSize, 0.0f);
        for(size_t i = 0; i < sample.size(); i++) {
            for(size_t j = 0; j < stateSize; j++) {
                avgState[j] += sample[i].state[j] * sampleSizeRecr;
            }
        }
        avgState.push_back(1.f); // bias
        actor->ApplyLoss(avgState, accumulatedGradient, actorLearningRate);

        // Calculate total gradient for logging
        float totalActorGradient = 0.0f;
        for(auto& g : accumulatedGradient) {
            totalActorGradient += std::abs(g);
        }

        // for(size_t j=0; j < actorGradient.size(); j++) {
        //     actorGradient[j] *= sampleSizeRecr; // Average gradient
        //     totalActorGradient += actorGradient[j];      // Sum of (negative) gradients
        // }
        // actorLossLog.push_back(actorLoss); // This would log a vector
        //  DEBUG_PRINTF("Actor loss: %f\n", totalLoss); // This prints sum of gradients, not a scalar loss
        // auto stateInput = sample[0].state; // Could use any sample's state here
        // stateInput.push_back(1.f);
        // actor->ApplyLoss(stateInput, actorGradient, learningRate);

        // Log the loss and total actor gradient
        // static APP_SRAM size_t msgCount = 0;
        // if (++msgCount % 20 == 0) { // Log every 10th optimisation
        //     if (m_scr_ptr) m_scr_ptr->post("Cr: " + String(loss, 4) + " Ac: " + String(totalActorGradient, 4));
        // }

        //back propagate the actor loss (apply gradients)
        // for(size_t i = 0; i < sample.size(); i++) {
        //     auto actorInput = sample[i].state;
        //     actorInput.push_back(bias); // Add bias to state for actor input

        //     // The actorLoss vector here is dQ/da. Actor update is theta_mu' = theta_mu + alpha_mu * grad_theta_mu Q(s, mu(s|theta_mu))
        //     // grad_theta_mu Q(s, mu(s|theta_mu)) = grad_a Q(s,a)|a=mu(s) * grad_theta_mu mu(s|theta_mu)
        //     // MLP::ApplyLoss expects the gradient of the loss function w.r.t the output of the network.
        //     // So, actorLoss (which is dQ/da) is the correct gradient to pass.
        //     actor->ApplyLoss(actorInput, actorLoss, learningRate);
        //     // delay(1); // This delay might be problematic
        // }

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
            const float noise = ou_noise.sample() * 0.4f;
            actorOutput[i] += noise;
            if (actorOutput[i] < 0.f) {
                actorOutput[i] = 0.f; // Ensure output is non-negative
            } else if (actorOutput[i] > 1.f) {
                actorOutput[i] = 1.f; // Ensure output does not exceed 1
            }
        }

        SendParamsToQueue(actorOutput);
        action = actorOutput;

        // for(size_t i=0; i < actorOutput.size(); i++) {
        //     const float noise = ou_noise.sample() * knobL; // ou_noise and knobL are not defined here
        //     actorOutput[i] += noise;
        // }
    }
}

void InterfaceRL::storeExperience(float reward) {
    // readAnalysisParameters(); // This would recursively call generateAction(true)
    std::vector<float> state = actorControlInput; // actorControlInput already includes bias if setup correctly
    // float bpf0 = READ_VOLATILE(sharedMem::f0); // bpf0 is read but not used

    //remove bias if it's part of actorControlInput for storage
    // The actorControlInput is stateSize + bias.
    // If 'state' in trainRLItem should not have bias, it needs to be removed.
    // The original code pops_back, assuming bias is the last element.
    if (!state.empty() && state.size() == stateSize + bias) { // Check if bias is likely present
       state.pop_back(); // remove bias if it was added for network input
    }


    for(size_t i=0; i < state.size(); i++) { // state here is without bias
        DEBUG_PRINTF("%f\t", state[i]);
    }
    DEBUG_PRINTLN();
    // nextState in DDPG is the state resulting from taking 'action' in 'state'.
    // Here, 'state' is used as 'nextState', which is common if the environment is static
    // or if the 'nextState' is the same as the current state for the purpose of this reward.
    // However, typically nextState would be s_t+1.
    // If actorControlInput is s_t+1, then the current state s_t needs to be stored from a previous step.
    // For now, assuming state is s_t and nextState is also s_t for this specific implementation detail.
    trainRLItem trainItem = {state, action, reward, state}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
    replayMem.add(trainItem, millis());
}
