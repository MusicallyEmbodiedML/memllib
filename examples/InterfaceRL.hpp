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

    void setup(size_t n_inputs, size_t n_outputs);

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
        }else{
            optimiseCounter++;
        }
    }

    void storeExperience(float reward);

    inline void randomiseTheActor()
    {
        actor->DrawWeights();
        actorTarget->DrawWeights();
    }

    inline void randomiseTheCritic()
    {
        critic->DrawWeights();
        criticTarget->DrawWeights();
    }

    inline void setOptimiseDivisor(size_t newDiv) {
        optimiseDivisor = newDiv;
    }
    inline void forgetMemory() {
        replayMem.clear();
    }

    // New methods
    void bind_RL_interface(display& scr_ref); // scr_ref is passed once here
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

    size_t stateSize;
    size_t actionSize;

    std::vector<size_t> actor_layers_nodes;
    std::vector<size_t> critic_layers_nodes;

    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;

    std::shared_ptr<MLP<float> > actor, actorTarget, critic, criticTarget;

    float discountFactor = 0.95;
    float learningRate = 0.005;
    float smoothingAlpha = 0.005;

    std::vector<float> action;

    ReplayMemory<trainRLItem> replayMem;

    std::vector<float> actorOutput, criticOutput;
    std::vector<float> criticInput;
    std::vector<float> actorControlInput;

    //std::vector<float> criticLossLog, actorLossLog, log1;


};

#endif // INTERFACERL_HPP