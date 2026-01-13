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
#include "../hardware/memlnaut/display/SingleSelectView.hpp"

#define RL_MEM __not_in_flash("rlmem")

struct trainStatelessRLItem {
    std::vector<float> input ;
    std::vector<float> action;
    float reward;
};

class InterfaceRL : public InterfaceBase
{
public:

    using OnMIDICtrlCallback = std::function<void(uint8_t)>;

    enum class INPUT_MODES {
        JOYSTICK,
        MACHINE_LISTENING,
        JOYSTICK_AND_MACHINE_LISTENING
    };

    enum class MEMORY_STORE_MODES {
        ADD,
        REPLACE_5_PERCENT,
        REPLACE_15_PERCENT,
        REWARD_DECAY_10_PERCENT,
        REWARD_DECAY_20_PERCENT
    };

   InterfaceRL() : InterfaceBase()
//    , ou_noise(0.02f, 0.0f, 0.2f, 0.001f, 0.0f)
{

    }
    void setup(size_t n_inputs, size_t n_outputs);

    void optimise();

    inline void setState(const size_t index, float value) {
        controlInput[index] = value;
        newInput = true;
    }

    void readAnalysisParameters(std::vector<float> params) override;

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

    void storeExperience(float reward, std::vector<float> &experienceState, std::vector<float> &experienceAction );

    #define randomWeightVariance 1.f

    inline void randomiseTheNetwork()
    {
        synthMapping->RandomiseWeightsAndBiasesLin(-0.9f,0.9f, 0, 0.5);
        newInput = true;
        resetMinMaxFlag = true;
    }


    inline void joltNetworks()
    {
        synthMapping->PurturbWeights(500);
        if (msgView) msgView->post("Jolting network");

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
        learningRateScaled = learningRate * scale;
        String msg = "LR scale: " + String(scale);
        if (msgView) msgView->post(msg);
    }

    void setRewardScaleInterf(float value);

    inline void setNoiseLevel(float level) {
        level *= 0.5f;
        if (level < 0.02f) {
            level = 0.f;
            if (msgView) msgView->post("Noise off");
        }
        String msg = "OU Sigma: " + String(level, 4);
        if (msgView) msgView->post(msg);
        for(auto& ou_noise: ou_noises) {
            ou_noise->setSigma(level);
        }
    }

    void bind_RL_interface(INPUT_MODES input_mode = INPUT_MODES::JOYSTICK, bool joystick4D = false);

    void bindInterface(INPUT_MODES input_mode = INPUT_MODES::JOYSTICK,bool joystick4D = false) {
        bind_RL_interface(input_mode, joystick4D);
    }

    void bindInterface(bool disable_joystick=false, bool joystick4D = false) {
        bind_RL_interface(disable_joystick ? INPUT_MODES::MACHINE_LISTENING : INPUT_MODES::JOYSTICK, joystick4D);
    }
    

    void bindUARTInput(std::shared_ptr<UARTInput> uart_input,
        const std::vector<size_t>& kUARTListenInputs)
    {
        DEBUG_PRINTLN("bindUARTInput not implemented yet");
    }
    
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf, bool enableFootcontroller=false);

    void trigger_like();
    void trigger_dislike();

    inline void getAction(std::vector<float> &out_action) {
        out_action = action;
    }

    void SetMIDI5Callback(OnMIDICtrlCallback _cb_) {
        midi5cb = _cb_;
    }    
    void SetMIDI6Callback(OnMIDICtrlCallback _cb_) {
        midi6cb = _cb_;
    }    

    // Display views
    std::shared_ptr<MessageView> msgView;
    std::shared_ptr<BlockSelectView> fileSaveView;
    std::shared_ptr<BlockSelectView> fileLoadView;
    std::shared_ptr<BarGraphView> nnInputsGraphView;
    std::shared_ptr<BarGraphView> nnOutputsGraphView;
    std::shared_ptr<RLStatsView> rlStatsView;
    std::shared_ptr<SingleSelectView> memoryStoreModeView;

protected:
    // Helper methods for trigger actions
    void _perform_like_action();
    void _perform_dislike_action();
    void _perform_randomiseRL_action();
    bool _save_RL_to_SD(String id);
    bool _load_RL_from_SD(String id);
    void _forget_replay_mem_interf();
    

private:

    OnMIDICtrlCallback midi5cb = nullptr;
    OnMIDICtrlCallback midi6cb = nullptr;

    static constexpr size_t bias=1;

    size_t optimiseDivisor = 1;
    size_t optimiseCounter = 0;
    bool newInput=false;

    bool actionBeingDragged=false;

    std::vector<size_t> itemsToRemove;

    size_t analysisParamsOffset = 0;

    MEMORY_STORE_MODES memoryStoreMode = MEMORY_STORE_MODES::ADD;
    std::array<String, 5> memOptions = {"Add", "Replace 5%", "Replace 15%", "Reward Decay 10%", "Reward Decay 20%"};
    void removeItemsAtDistance(std::vector<float> &experienceState, const float distThreshold);
    void decayItemsAtDistance(std::vector<float> &experienceState, const float distThreshold);





    std::vector<size_t> layers_nodes;

    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;

    std::shared_ptr<MLP<float> > synthMapping;

    float learningRate = 1e-3;
    float learningRateScaled = learningRate;
    std::vector<float> action;


    ReplayMemory<trainStatelessRLItem> replayMem;
    static constexpr size_t memoryLimit = 64;
    static constexpr size_t batchSize = 8;

    std::vector<float> mappingOutput;
    std::vector<float> controlInput;
    std::vector<float> savedAction;


    float rewardScale = 1.0f;

    // OrnsteinUhlenbeckNoise ou_noise;
    std::vector<std::unique_ptr<OrnsteinUhlenbeckNoise>> ou_noises;

    bool resetMinMaxFlag = false;

    spin_lock_t *mlpActive;

};

#endif // INTERFACERL_HPP