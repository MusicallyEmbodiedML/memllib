#ifndef INTERFACERL_HPP
#define INTERFACERL_HPP

#include "../interface/InterfaceBase.hpp"

#include "../../memlp/MLP.h"
#include "../../memlp/ReplayMemory.hpp"
#include "../../memlp/OrnsteinUhlenbeckNoise.h"
#include <memory>
#include "../utils/sharedMem.hpp"

#include "../PicoDefs.hpp"
#include "../hardware/memlnaut/display.hpp" // Added include

#include "../interface/UARTInput.hpp"
#include "../interface/MIDIInOut.hpp"

#define RL_MEM __not_in_flash("rlmem")

#define XIASRI    0


struct trainRLItem {
    std::vector<float> state ;
    std::vector<float> action;
    float reward;
    std::vector<float> nextState;
};


class InterfaceRL : public InterfaceBase
{
public:
   InterfaceMinimaRL() : InterfaceBase(), ou_noise(0.02f, 0.0f, 0.2f, 0.001f, 0.0f) {

    }
    void setup(size_t n_inputs, size_t n_outputs);
    void setup(size_t n_inputs, size_t n_outputs, std::shared_ptr<display> disp);

    void optimise();

    inline void setState(const size_t index, float value) {
        actorControlInput[index] = value;
        newInput = true;
    }

    void readAnalysisParameters();

    void generateAction(bool donthesitate=false);

    inline void optimiseSometimes() {
        if (optimiseCounter>=optimiseDivisor) {
            optimise();
            optimiseCounter=0;
            newInput = true;
        }else{
            optimiseCounter++;
        }
    }

    void storeExperience(float reward);

    inline void randomiseTheActor()
    {
        actor->DrawWeights();
        actorTarget->DrawWeights();
        newInput = true;
    }

    inline void randomiseTheCritic()
    {
        critic->DrawWeights();
        criticTarget->DrawWeights();
        newInput = true;
    }

    inline void setOptimiseDivisor(size_t newDiv) {
        optimiseDivisor = newDiv;
    }

    inline void forgetMemory() {
        replayMem.clear();
    }

    inline void setRewardScale(float scale) {
        rewardScale = scale;
    }

    inline void setDiscountFactor(float factor) {
        discountFactor = factor;
    }

    inline void setNoiseLevel(float level) {
        ou_noise.setSigma(level * 0.5f);
    }

    // New methods
    void bind_RL_interface(display& scr_ref, bool disable_joystick = false); // scr_ref is passed once here
    void bindInterface(bool disable_joystick = false)
    {
        if (m_scr_ptr) {
            bind_RL_interface(*m_scr_ptr, disable_joystick); // Use the stored pointer to display
        } else {
            Serial.println("Display pointer is null, cannot bind interface.");
        }
    }
    void bindUARTInput(std::shared_ptr<UARTInput> uart_input,
        const std::vector<size_t>& kUARTListenInputs)
    {
        Serial.println("bindUARTInput not implemented yet");
    }
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf)
    {
        Serial.println("bindMIDI not implemented yet");
    }

    void trigger_like();
    void trigger_dislike();
    void trigger_randomiseRL();

protected:
    // Helper methods for trigger actions
    void _perform_like_action();
    void _perform_dislike_action();
    void _perform_randomiseRL_action();

private:
    display* m_scr_ptr = nullptr; // Pointer to the display object
    static constexpr size_t bias=1;

    size_t optimiseDivisor = 40;
    size_t optimiseCounter = 0;

    bool newInput=false;

    const std::vector<ACTIVATION_FUNCTIONS> layers_activfuncs = {
        RELU, RELU, SIGMOID
    };

    size_t controlSize;
    size_t stateSize;
    size_t actionSize;

    std::vector<size_t> actor_layers_nodes;
    std::vector<size_t> critic_layers_nodes;

    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;

    std::shared_ptr<MLP<float> > actor, actorTarget, critic, criticTarget;

    float discountFactor = 0.1f;
    float actorLearningRate = 1e-4;
    float criticLearningRate = 1e-4;
    float smoothingAlpha = 0.01f;

    std::vector<float> action;

    ReplayMemory<trainRLItem> replayMem;

    std::vector<float> actorOutput, criticOutput;
    std::vector<float> criticInput;
    std::vector<float> actorControlInput;

    //std::vector<float> criticLossLog, actorLossLog, log1;
    float rewardScale = 1.f;
  
    OrnsteinUhlenbeckNoise ou_noise;
};

#endif // INTERFACERL_HPP