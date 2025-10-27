#ifndef INTERFACERL_HPP
#define INTERFACERL_HPP

#include "../interface/InterfaceBase.hpp"

#include "../../memlp/MLP.h"
#include "../../memlp/ReplayMemory.hpp"
#include "../../memlp/OrnsteinUhlenbeckNoise.h"
#include <memory>
#include "../utils/sharedMem.hpp"

#include "../PicoDefs.hpp"
//#include "../hardware/memlnaut/display.hpp"
#include "../hardware/memlnaut/display/MessageView.hpp"

#include "../interface/UARTInput.hpp"
#include "../interface/MIDIInOut.hpp"

#include "../hardware/memlnaut/display/MessageView.hpp"
#include "../hardware/memlnaut/display/BarGraphView.hpp"
#include "../hardware/memlnaut/display/BlockSelectView.hpp"
#include "../hardware/memlnaut/display/RLStatsView.hpp"
#include "../hardware/memlnaut/display/VoiceSpaceSelectView.hpp"

#define RL_MEM __not_in_flash("rlmem")

//#define XIASRI 1

struct trainRLItem {
    std::vector<float> state ;
    std::vector<float> action;
    float reward;
    std::vector<float> nextState;
};


class InterfaceRL : public InterfaceBase
{
public:

    using OnMIDICtrlCallback = std::function<void(uint8_t)>;

   InterfaceRL() : InterfaceBase()
//    , ou_noise(0.02f, 0.0f, 0.2f, 0.001f, 0.0f)
{

    }
    void setup(size_t n_inputs, size_t n_outputs);

    void optimise();

    inline void setState(const size_t index, float value) {
        actorControlInput[index] = value;
        newInput = true;
    }

    void readAnalysisParameters(std::vector<float> params);

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

    #define randomWeightVariance 1.f

    inline void randomiseTheActor()
    {
        actor->DrawWeights(0.8f);
        actorTarget->DrawWeights(0.8f);
        // actor->InitXavier();
        // actorTarget->InitXavier();

        newInput = true;
        resetMinMaxFlag = true;
    }

    inline void randomiseTheCritic()
    {
        critic->DrawWeights(0.8f);
        criticTarget->DrawWeights(0.8f);
        // critic->InitXavier();
        // criticTarget->InitXavier();
        newInput = true;
        resetMinMaxFlag = true;
    }

    inline void joltNetworks()
    {
        actor->PurturbWeights(500);
        critic->PurturbWeights(500);
        if (msgView) msgView->post("Jolting Actor and Critic nets");

    }

    inline void setOptimiseDivisor(size_t newDiv) {
        optimiseDivisor = newDiv;
    }

    void setOptimiseDivisorInterf(float value);

    inline void forgetMemory() {
        replayMem.clear();
    }

    inline void setRewardScale(float scale) {
        rewardScale = scale;
    }

    inline void setLRScale(const float scale) {
        actorLearningRateScaled = actorLearningRate * scale;
        criticLearningRateScaled = criticLearningRate * scale;
        String msg = "LR scale: " + String(scale);
        if (msgView) msgView->post(msg);
    }

    void setRewardScaleInterf(float value);

    inline void setDiscountFactor(float factor) {
        discountFactor = factor;
    }

    inline void setNoiseLevel(float level) {
        level *= 0.5f;
        if (level < 0.05f) {
            level = 0.f;
            if (msgView) msgView->post("Noise off");
        }
        String msg = "OU Sigma: " + String(level, 4);
        if (msgView) msgView->post(msg);
        for(auto& ou_noise: ou_noises) {
            ou_noise->setSigma(level);
        }
    }

    void bind_RL_interface(bool disable_joystick = false);

    __force_inline void bindInterface(bool disable_joystick = false) {
        bind_RL_interface(disable_joystick);
    }

    void bindUARTInput(std::shared_ptr<UARTInput> uart_input,
        const std::vector<size_t>& kUARTListenInputs)
    {
        DEBUG_PRINTLN("bindUARTInput not implemented yet");
    }
    
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf);

    void trigger_like();
    void trigger_dislike();
    // void trigger_randomiseRL();

    inline void getAction(std::vector<float> &out_action) {
        out_action = action;
    }

    void SetMIDI5Callback(OnMIDICtrlCallback _cb_) {
        midi5cb = _cb_;
    }    
    void SetMIDI6Callback(OnMIDICtrlCallback _cb_) {
        midi6cb = _cb_;
    }    
protected:
    // Helper methods for trigger actions
    void _perform_like_action();
    void _perform_dislike_action();
    void _perform_randomiseRL_action();
    bool _save_RL_to_SD(String id);
    bool _load_RL_from_SD(String id);
    void _randomise_critic_interf();
    void _forget_replay_mem_interf();

private:

    OnMIDICtrlCallback midi5cb = nullptr;
    OnMIDICtrlCallback midi6cb = nullptr;

    static constexpr size_t bias=1;

    size_t optimiseDivisor = 40;
    size_t optimiseCounter = 0;
    bool newInput=false;


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
    float criticLearningRate = 1e-3;
    float smoothingAlpha = 0.01f;
    float actorLearningRateScaled = actorLearningRate;
    float criticLearningRateScaled = criticLearningRate;
    std::vector<float> action;

    ReplayMemory<trainRLItem> replayMem;
    static constexpr size_t memoryLimit = 64;
    static constexpr size_t batchSize = 4;

    std::vector<float> actorOutput, criticOutput;
    std::vector<float> criticInput;
    std::vector<float> actorControlInput;

    //std::vector<float> criticLossLog, actorLossLog, log1;
    float rewardScale = 0.8f;

    // OrnsteinUhlenbeckNoise ou_noise;
    std::vector<std::unique_ptr<OrnsteinUhlenbeckNoise>> ou_noises;

    // Display views
    std::shared_ptr<MessageView> msgView;
    std::shared_ptr<BlockSelectView> fileSaveView;
    std::shared_ptr<BlockSelectView> fileLoadView;
    std::shared_ptr<BarGraphView> nnInputsGraphView;
    std::shared_ptr<BarGraphView> nnOutputsGraphView;
    std::shared_ptr<RLStatsView> rlStatsView;
    std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
    bool resetMinMaxFlag = false;

};

#endif // INTERFACERL_HPP