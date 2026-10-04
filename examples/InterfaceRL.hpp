#ifndef INTERFACERL_HPP
#define INTERFACERL_HPP

#include "../interface/InterfaceBase.hpp"

#include "../../memlp/StaticMLP.h"
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
#include "../hardware/memlnaut/display/InputsView.hpp"
#include "../hardware/memlnaut/display/RLView.hpp"
#include "../hardware/memlnaut/display/BlockSelectView.hpp"
#include "../hardware/memlnaut/display/SingleSelectView.hpp"
#include "../hardware/memlnaut/display/RotarySelectView.hpp"
#include "../hardware/memlnaut/display/NameInputView.hpp"
#include "../hardware/memlnaut/display/CCSelectView.hpp"
#include "InterfaceRLFileFormat.hpp"

#define RL_MEM __not_in_flash("rlmem")

struct trainStatelessRLItem {
    std::vector<float> input ;
    std::vector<float> action;
    float reward;
};

// Non-template base holding the nested types + constants that callers reference
// without a template argument (e.g. InterfaceRLBase::INPUT_MODES in the modes
// and the .ino). Because this (and InterfaceBase) are *non-dependent* bases of
// InterfaceRL<N_OUTPUTS>, the template sees every inherited member by ordinary
// lookup — no this->/using-declarations required.
class InterfaceRLBase : public InterfaceBase
{
public:
    using OnMIDICtrlCallback = std::function<void(uint8_t)>;

    // Training data type (independent of the network's output width).
    using training_pair_t = std::pair<std::vector<std::vector<float>>,
                                      std::vector<std::vector<float>>>;

    static constexpr size_t kMaxNNInputs = 10;

    enum class INPUT_MODES {
        JOYSTICK,
        MACHINE_LISTENING,
        JOYSTICK_AND_MACHINE_LISTENING,
        SERIAL_INPUT
    };

    enum class INPUT_SOURCE : uint8_t {
        JOYSTICK_3D = 0,
        JOYSTICK_4D,
        MACHINE_LISTENING,
        MIDI_1CC,
        MIDI_3CC,
        MIDI_8CC,
        COMBINED,
        COUNT
    };

    enum class MEMORY_STORE_MODES {
        ADD,
        REPLACE_5_PERCENT,
        REPLACE_10_PERCENT,
        REPLACE_15_PERCENT,
        REWARD_DECAY_10_PERCENT,
        REWARD_DECAY_20_PERCENT
    };

    // What a 'no' means. Selected on the "Dislike Mode" screen, persisted to flash.
    //  CURRENT        original behaviour: an unbounded push away from the liked centroid
    //  BOUNDED_BLAME  move a bounded distance away, mostly reverting the params that differ
    //                 most from the nearest like ("what you just changed is wrong")
    //  REROLL         jump to a fresh sound here ("try something else")
    //  ESCALATE       repeated 'no's in one place: bounded nudge, then retreat to the
    //                 nearest like, then re-roll
    //  BORROW         rebuild the sound here from qualities of nearby liked sounds, keeping
    //                 a random subset of its own params so each 'no' gives a new variation
    enum class DISLIKE_MODES : uint8_t {
        CURRENT = 0,
        BOUNDED_BLAME,
        REROLL,
        ESCALATE,
        BORROW,
        COUNT
    };

    // Input-source state is independent of the network's output width, so it
    // lives in the base: this lets N-agnostic helpers (e.g. MachineListeningMixin)
    // hold an InterfaceRLBase* and still query/configure the input source.
    INPUT_SOURCE getInputSource() const   { return input_source_; }
    void setHasMachineListening(bool v)   { hasMachineListening_ = v; }

    static const char* inputSourceName(INPUT_SOURCE s) {
        static const char* names[] = {
            "3D Joystick", "4D Joystick", "Machine Listen",
            "MIDI Mod Whl", "MIDI 3 CC", "MIDI 8 CC", "Combined"
        };
        const size_t i = static_cast<size_t>(s);
        return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
    }
    // Short (<= 4 char) labels for the machine-listening inputs, shown on the NN Inputs
    // screen. Set by whoever owns the analysis (e.g. MachineListeningMixin).
    void setMLInputLabels(const std::vector<String>& labels) { mlLabels_ = labels; }

protected:
    INPUT_SOURCE input_source_ = INPUT_SOURCE::JOYSTICK_3D;
    bool hasMachineListening_ = false;
    std::vector<String> mlLabels_{"ML1", "ML2", "ML3", "ML4", "ML5", "ML6"};
};

// The RL interface. N_OUTPUTS (the active mode's parameter count) is fixed at
// compile time, so the synth-mapping network is a static-memory StaticMLP with
// no heap allocation. The mode declares e.g. InterfaceRL<MyApp::kN_Params>.
// N_INPUTS defaults to the shared kMaxNNInputs (10) so every existing mode is
// unaffected; a mode that needs a wider input layer (e.g. more raw MIDI CC
// inputs than the built-in INPUT_SOURCE options support) can opt into a wider
// net explicitly, e.g. InterfaceRL<16, 16>.
template<size_t N_OUTPUTS, size_t N_INPUTS = InterfaceRLBase::kMaxNNInputs>
class InterfaceRL : public InterfaceRLBase
{
public:

    // Compile-time mapping network: N_INPUTS -> 16 -> 16 -> N_OUTPUTS.
    using SynthMLP = smlp::StaticMLP<float,
        smlp::Layout<N_INPUTS, 16, 16, N_OUTPUTS>,
        smlp::Activations<RELU, RELU, HARDSIGMOID>,
        loss::LOSS_FUNCTIONS::LOSS_MSE>;

   InterfaceRL() : InterfaceRLBase()
//    , ou_noise(0.02f, 0.0f, 0.2f, 0.001f, 0.0f)
{

    }
    void setup(size_t n_inputs, size_t n_outputs, bool addMessageView = true);

    void optimise();

    // Train on demand: instead of training every cycle, settle (no training) once the
    // error on the liked memories is low or has stopped improving, and wake again when it
    // rises clearly above where it settled or the data/weights change (like, dislike,
    // drag, jolt, scramble, model load). A settled net holds still, so noise roams over a fixed mapping
    // instead of being pulled back, and core 0 is freed. Off by default (per mode opt-in).
    void setTrainOnDemand(bool on) { trainOnDemand_ = on; wakeTraining(); }
    bool isTrainingSettled() const { return trainOnDemand_ && trainSettled_; }

    inline void setState(const size_t index, float value) {
        controlInput[index] = value;
        newInput = true;
    }

    // Force the next loop to regenerate + re-send the action, even if no input changed.
    // Use when an output-stage parameter (e.g. a fade/home value) changes.
    inline void markInputDirty() { newInput = true; }

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
        synthMapping.RandomiseWeightsAndBiasesLin(-0.9f,1.1f, -0.9f, 0.3f);
        synth_.clear();
        wakeTraining();
        newInput = true;
        resetMinMaxFlag = true;
    }


    inline void setOptimiseDivisor(size_t newDiv) {
        optimiseDivisor = newDiv;
    }

    void setOptimiseDivisorInterf(float value);

    inline void forgetMemory() {
        replayMem.clear();
        synth_.clear();
        locDirty_ = true;
        wakeTraining();  // refresh counts; with nothing to learn it settles again at once
        dislikeTargets_.clear();
        escalateStage_ = 0;
        lastDislikeInput_.clear();
    }

    DISLIKE_MODES getDislikeMode() const { return dislikeMode_; }
    void setDislikeMode(DISLIKE_MODES m, bool persist = true);

    inline void setRewardScale(float scale) {
        rewardScale = scale;
    }

    inline void setLRScale(const float scale) {
        learningRateScaled = learningRate * scale;  // knob at 0 -> LR 0 -> training off (intended)
        String msg = "LR scale: " + String(scale);
        if (msgView) msgView->post(msg);
    }

    void setRewardScaleInterf(float value);

    inline void setNoiseLevel(float level) {
        // Knob [0,1] -> roaming amplitude (the OU walk's stationary std) in param space.
        // theta/dt (set in setup) fix the smoothness; this only sets how far each param
        // drifts from the mapping output. Low = gentle local wander; full = slow sweeps
        // across the whole [0,1] range. kMaxAmplitude sets the "depth": higher reaches
        // deeper into the param space (and interacts more with the [0,1] rails via the
        // gentle reflection in generateAction), lower keeps it shallower/more local.
        constexpr float kMaxAmplitude = 0.65f;
        float amplitude = level * kMaxAmplitude;
        noiseAmp_ = amplitude < 0.01f ? 0.f : amplitude;  // depth of the weight noise
        if (amplitude < 0.01f) {
            amplitude = 0.f;
            if (msgView) msgView->post("Noise off");
        } else {
            String msg = "Explore amount: " + String(amplitude, 3);
            if (msgView) msgView->post(msg);
        }
        // Learning stays active during exploration on purpose: likes/dislikes given while
        // the noise roams are what steer the network toward sounds the player wants.
        if (nnOutputsGraphView) nnOutputsGraphView->setNoiseActive(amplitude > 0.f);
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
        uart_input->SetCallback([this](size_t channel, float value) {
            // Serial.println("UART input: " + String(channel) + " value: " + String(value));
            if (channel < controlInput.size()) {
                setState(channel, value);
            }
        });
    }
    
    void bindMIDI(std::shared_ptr<MIDIInOut> midi_interf, bool enableFootcontroller=false);

    void setModeInfo(const String& modeRoot, const String& modeTag);

    using ExtraSaveDataFn = std::function<std::vector<uint8_t>()>;
    using ExtraLoadDataFn = std::function<void(const uint8_t*, uint16_t, uint16_t)>;
    void setExtraSaveCallback(ExtraSaveDataFn fn) { _extraSaveFn = fn; }
    void setExtraLoadCallback(ExtraLoadDataFn fn) { _extraLoadFn = fn; }

    using RVCallback = std::function<void(float)>;
    void setRVX1Override(RVCallback fn) { rvX1Override = std::move(fn); }
    void setRVY1Override(RVCallback fn) { rvY1Override = std::move(fn); }
    void setRVZ1Override(RVCallback fn) { rvZ1Override = std::move(fn); }

    void setActiveDims(std::vector<bool> dims) { activeDims_ = std::move(dims); }

    std::function<void(std::vector<float>&)> inputInjectionHook;

    void trigger_like();
    void trigger_dislike();

    inline void getAction(std::vector<float> &out_action) {
        out_action = action;
    }

    size_t getActiveInputCount() const {
        switch (input_source_) {
            case INPUT_SOURCE::JOYSTICK_3D:       return 3;
            case INPUT_SOURCE::JOYSTICK_4D:       return 4;
            case INPUT_SOURCE::MACHINE_LISTENING: return 6;
            case INPUT_SOURCE::MIDI_1CC:          return 1;
            case INPUT_SOURCE::MIDI_3CC:          return 3;
            case INPUT_SOURCE::MIDI_8CC:          return 8;
            case INPUT_SOURCE::COMBINED:          return N_INPUTS;
            default:                              return N_INPUTS;
        }
    }
    const std::vector<float>& getControlInput() const { return controlInput; }

    // Recompute the unused-input pad value. Done only when the input mode changes (not per
    // frame). Scales as 1.1/n_unused so the total constant injected into layer 1 stays
    // bounded regardless of how many dims are unused — avoids over-driving the net.
    void updateUnusedInputDefault() {
        const size_t used   = getActiveInputCount();
        const size_t unused = (used < N_INPUTS) ? (N_INPUTS - used) : 0;
        unusedInputDefault_ = (unused > 0) ? (1.1f / static_cast<float>(unused)) : 0.f;
    }

    // persist=false applies the change in memory + updates the bar graph without writing
    // flash. A flash write stalls XIP execution on the RP2040 (blanking the display), so
    // the rotary-driven path applies immediately and saves only once the selector loses
    // focus (see loopCallback).
    void setInputSource(INPUT_SOURCE src, bool persist = true) {
        input_source_ = src;
        updateUnusedInputDefault();
        if (persist) saveInputSource();
        configureInputsView();
    }

    // ISR-safe entry point (the rotary-encoder dispatch runs in interrupt context).
    // setInputSource() does heap allocation (bar-graph resize), SPI (fillRect) and flash
    // file IO — all unsafe in an ISR — so only record the request here and let the main
    // loop apply it via the pendingInputSourceChange_ handler in bind_RL_interface().
    void requestInputSource(INPUT_SOURCE src) {
        pendingInputSource_ = src;
        pendingInputSourceChange_ = true;
    }
    void addInputSourceView(bool includeCCSelect = true);

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
    std::shared_ptr<NameInputView> nameInputView;
    std::shared_ptr<InputsView> nnInputsGraphView;
    std::shared_ptr<RLView> nnOutputsGraphView;
    std::shared_ptr<SingleSelectView> memoryStoreModeView;
    std::shared_ptr<CCSelectView> ccSelectView;
    std::shared_ptr<RotarySelectView> dislikeModeView;

    const std::vector<float>& getLastAction() const { return action; }

protected:
    // Helper methods for trigger actions
    void _perform_like_action();
    void _perform_dislike_action();
    void _perform_randomiseRL_action();
    bool _save_RL_to_SD(String id);
    bool _load_RL_from_SD(String id);
    bool writeLikes(File& file);
    bool readLikes(File& file);  // replaces the replay memory with the saved likes
    void _forget_replay_mem_interf();
    void _saveSlotNames();
    void _loadSlotNames();

    static constexpr int kNumSlots = 12;
    static constexpr uint16_t kSaveAccent = TFT_ORANGE;
    static constexpr uint16_t kLoadAccent = 0x929F;  // violet
    // Highlight the slot of the model now in use, on both the save and load screens.
    void markCurrentSlot(int slot) {
        for (int i = 0; i < kNumSlots; i++) {
            if (fileSaveView) fileSaveView->setAltState(i, i == slot);
            if (fileLoadView) fileLoadView->setAltState(i, i == slot);
        }
    }
    String slotNames[kNumSlots];
    int pendingSaveSlot = -1;
    

private:

    OnMIDICtrlCallback midi5cb = nullptr;
    OnMIDICtrlCallback midi6cb = nullptr;

    RVCallback rvX1Override;
    RVCallback rvY1Override;
    RVCallback rvZ1Override;

    static constexpr size_t bias=1;

    size_t optimiseDivisor = 1;
    size_t optimiseCounter = 0;
    bool newInput=false;

    bool actionBeingDragged=false;

    std::vector<size_t> itemsToRemove;

    float raw_joystick_[4] = {};
    float raw_ml_[6]       = {};
    float raw_midi_[8]     = {};
    // Constant used to pad the unused NN input dims; recomputed only on input-mode change.
    float unusedInputDefault_ = 0.5f;

    static constexpr const char* kInputSourceFile = "/input_source.bin";
    void assembleInputs();
    void copyAndZero(const float* src, size_t n);
    void saveInputSource();
    void configureInputsView();  // labels/mode of the NN Inputs screen for input_source_
    void pushMemoryPointsToInputsView();
    uint32_t lastMemPointsMs_ = 0;
    void loadInputSource();
    // Value currently on flash, so a commit that changes nothing skips the write.
    INPUT_SOURCE savedInputSource_ = INPUT_SOURCE::COUNT;
    void saveCCNumbers();
    void loadCCNumbers();

    size_t analysisParamsOffset = 0;

    MEMORY_STORE_MODES memoryStoreMode = MEMORY_STORE_MODES::REPLACE_10_PERCENT;
    std::array<String, 6> memOptions = {"Add", "Replace 5%", "Replace 10%", "Replace 15%", "Reward Decay 10%", "Reward Decay 20%"};
    // Dislike repulsion: how far a 'no' moves the disliked action's training target away
    // from the liked region (untapered), and the negative-batch LR base. Bigger = a 'no'
    // slides the sound clearly further away; >1 tends to push params to the [0,1] rails.
    static constexpr float kGeometricPushScale = 1.0f;
    static constexpr float kNegLRBase = 1.5f;  // negLRRatio = kNegLRBase - 0.4*negFraction
    // A 'no' pushes at full strength for this long (wall-clock), then expires — no decay.
    // The number of optimise cycles within the window depends on the mode's NN update rate.
    static constexpr uint32_t kDislikeLifetimeMs = 2500;
    static constexpr size_t kCentroidK = 4;
    std::vector<bool> activeDims_;
    bool removeItemsAtDistance(std::vector<float> &experienceState, const float distThreshold, const float reward);
    void decayItemsAtDistance(std::vector<float> &experienceState, const float distThreshold);

    // --- Dislike modes other than CURRENT ---------------------------------------------
    // These never store a negative in replayMem. A 'no' becomes a short-lived training
    // target for the input where it was given, fixed at press time (not re-rolled per
    // cycle), kept inside [kTargetLo, kTargetHi] so the hard-sigmoid output never lands in
    // its flat, zero-gradient region. Each target retires once its goal is met, or after
    // kDislikeTargetLifetimeMs, so a 'no' always has an end condition.
    // Distances in output space are RMS over the active dims, so they mean the same thing
    // regardless of the mode's param count.
    struct DislikeTarget {
        enum class Kind : uint8_t { REPEL, SEEK };
        std::vector<float> input;
        std::vector<float> origin;  // the disliked output
        std::vector<float> target;
        uint32_t t0;
        Kind kind;  // REPEL: done once the output is kRepelMargin from origin
                    // SEEK:  done once the output is within kSeekEpsilon of target
    };
    std::vector<DislikeTarget> dislikeTargets_;
    DISLIKE_MODES dislikeMode_ = DISLIKE_MODES::CURRENT;
    volatile bool pendingDislikeModeChange_{false};
    DISLIKE_MODES pendingDislikeMode_{DISLIKE_MODES::CURRENT};
    static constexpr const char* kDislikeModeFile = "/dislike_mode.bin";
    // ESCALATE: stage of the current run of 'no's (0 nudge, 1 retreat, 2+ re-roll)
    size_t escalateStage_ = 0;
    uint32_t lastDislikeMs_ = 0;
    std::vector<float> lastDislikeInput_;

    static constexpr float kTargetLo = 0.05f;
    static constexpr float kTargetHi = 0.95f;
    static constexpr float kRepelMargin = 0.12f;     // RMS: how far a nudge must move the sound
    static constexpr float kRepelStep = 0.16f;       // RMS: nudge target distance (> margin)
    static constexpr float kSeekEpsilon = 0.03f;     // RMS: a retreat/re-roll has arrived
    static constexpr float kRetreatJitter = 0.03f;   // per-dim jitter on a retreat target
    static constexpr float kRerollMinDist = 0.25f;   // RMS: a re-roll must be at least this new
    static constexpr float kRerollSpread = 0.2f;     // per-dim spread around a liked sound
    static constexpr float kBorrowKeepFraction = 0.4f;  // share of params the 'no'd sound keeps
    static constexpr float kBorrowInputSigma = 0.3f;    // input-space reach of "nearby" likes
    static constexpr float kBorrowJitter = 0.02f;       // per-dim jitter on borrowed values
    static constexpr float kDislikeLR = 1.5f;        // x the base LR, like kNegLRBase
    static constexpr uint32_t kDislikeTargetLifetimeMs = 4000;
    static constexpr size_t kMaxDislikeTargets = 8;
    static constexpr float kDislikeInputRadius = 0.10f;  // a new 'no' replaces targets this close
    static constexpr uint32_t kEscalateWindowMs = 6000;  // a 'no' within this, nearby, escalates
    static constexpr float kEscalateInputRadius = 0.15f;

    bool isActiveDim(size_t j) const {
        return activeDims_.empty() || (j < activeDims_.size() && activeDims_[j]);
    }

    // --- Focus as a local edit ----------------------------------------------------------
    // With some parameter groups focused (others frozen by the mode's FocusManager), the
    // player is editing just those parameters, here. So while focus is on:
    //  - exploration noise isn't suppressed near likes (focus already protects the rest)
    //  - a jolt also seeds points at/around the current input (focused dims re-rolled)
    //  - likes near the current input stop asserting their focused dims (their target
    //    there is whatever the net plays now), so an edit isn't pulled back to the old
    //    value; they keep holding everything else. Re-liking replaces the old like.
    //  - a dislike never deletes a nearby like, only moves the focused dims
    // Releasing focus restores the normal behaviour.
    static constexpr float kFocusEditRadius = 0.15f;  // "near the current input" (per sqrt dim)
    static constexpr int kFocusLocalSeeds = 3;        // jolt points at/around the input
    static constexpr float kFocusSeedJitter = 0.08f;  // spread of those points
    bool focusActive() const {
        for (bool a : activeDims_) if (!a) return true;
        return false;
    }
    bool nearCurrentInput(const std::vector<float>& x) const;
    // Training target for liked memory item: its action, except (under focus) a like near
    // the current input releases its focused dims to the net's current output there.
    const std::vector<float>& likeTarget(size_t i);
    std::vector<float> likeTargetBuf_;
    float rmsDistActive(const std::vector<float>& a, const std::vector<float>& b) const;
    int nearestLikeIndex(const std::vector<float>& input) const;  // -1 if none
    std::vector<float> stepTarget(const std::vector<float>& origin, std::vector<float> dir, float rmsStep) const;
    std::vector<float> randomUnitDir(size_t n) const;
    void addDislikeTarget(DislikeTarget&& t);
    void removeEndorsingLikes();
    void dislikeNudge(bool blame);
    void dislikeRetreat();
    void dislikeReroll();
    void dislikeBorrow();
    void handleDislike();
    void trainDislikeTargets(float lr);
    void loadDislikeMode();
    void saveDislikeMode();

    // --- Exploration noise (Z knob): weight-space noise -----------------------------
    // Playback uses the net with its last layer perturbed along kWeightDirs fixed random
    // directions, each scaled by its own slow drift: the whole mapping morphs smoothly and
    // consistently across inputs, while training and the stored weights stay clean. The
    // depth shrinks near liked inputs (explore the gaps, keep the likes).
    static constexpr size_t kHidden = 16;          // hidden layer width (see SynthMLP)
    static constexpr size_t kWeightDirs = 6;
    static constexpr float kNearFloor = 0.2f;      // noise scale right on a liked input
    static constexpr float kNearRadius = 0.25f;    // full noise beyond this (per sqrt dim)
    float noiseAmp_ = 0.f;
    std::vector<OrnsteinUhlenbeckNoise> exploreOU_;            // unit-std drift per direction
    int8_t weightDirs_[kWeightDirs][N_OUTPUTS][kHidden] = {};  // +-1, fixed at setup
    // The drifts move slowly, so the combined offset dW = sum_k c_k D_k is rebuilt only
    // every kNoiseRebuildMs (stepping the drifts once per elapsed control cycle, so their
    // speed is unchanged); each inference is then one N_OUTPUTS x kHidden product.
    static constexpr uint32_t kNoiseRebuildMs = 50;
    static constexpr uint32_t kControlPeriodMs = 5;
    float noiseW_[N_OUTPUTS][kHidden] = {};
    uint32_t noiseWMs_ = 0;
    bool noiseWValid_ = false;
    void rebuildNoiseW();
    // noiseLocality() cache: recomputed when the input or the likes change.
    std::vector<float> locInput_;
    size_t locMemSize_ = SIZE_MAX;
    float locValue_ = 1.f;
    bool locDirty_ = true;
    void initExploreNoise();
    float noiseLocality(const std::vector<float>& x);
    void noisyForward(std::vector<float>& out);

    // --- Jolt (B2): synthetic pseudo-likes ------------------------------------------
    // A jolt plants kSynthPoints synthetic training points spread through the playable
    // input range but clear of the liked inputs, with target sounds well away from what
    // the net plays there now, and trains towards them (a burst at a high LR until they
    // are mostly reached, then as anchors at normal weight) until the next jolt.
    // New sounds appear in the untaught regions while the likes keep training at their own
    // inputs, so preferences are preserved. Liking a jolted sound keeps it for good.
    struct SynthPoint {
        std::vector<float> input;
        std::vector<float> target;
    };
    static constexpr int kSynthPoints = 8;
    static constexpr int kSynthCandidates = 32;
    // Inputs from the playable range (sticks rarely reach their ends; the furthest-from-
    // likes corners are where nobody plays, so jolts there sounded subtle).
    static constexpr float kSynthInputLo = 0.1f;
    static constexpr float kSynthInputHi = 0.9f;
    static constexpr float kSynthClearance = 0.2f;   // min distance from likes (per sqrt dim)
    static constexpr float kSynthTargetLo = 0.05f;   // keep targets off the flat ends
    static constexpr float kSynthTargetHi = 0.95f;
    static constexpr float kSynthBurstLR = 10.f;     // burst at this x the base LR...
    static constexpr float kSynthBurstDone = 0.25f;  // ...until the error is this x where it began
    static constexpr uint32_t kSynthBurstMaxMs = 8000;  // (or this long)
    static constexpr float kSynthHoldLR = 1.f;       // then as anchors at this x
    float synthSeedError_ = 0.f;
    bool synthBurst_ = false;
    std::vector<SynthPoint> synth_;
    uint32_t synthSeedMs_ = 0;
    volatile bool pendingSeed_ = false;  // set from the B2 callback (ISR-safe)
    void seedJolt();
    void rollSynthTarget(SynthPoint& sp);  // big-change target for the focused dims
    // Dislikes vs exploration: a dislike near a jolt point re-rolls that point's sound
    // (and replaces the normal dislike, which would fight it); a dislike while noise is
    // on also jumps the noise to the other side of the mapping (it, not the net, took
    // you there).
    static constexpr float kSynthDislikeRadius = 0.2f;  // per sqrt input dim
    static constexpr float kNoiseDislikeJump = 0.5f;    // min jump of each coefficient
    bool rerollNearbySynth();
    void jumpNoise();
    bool trainSynth(float lr);  // returns true during the post-jolt burst
    float synthError();  // mean MSE to the synthetic targets (no update)
    DISLIKE_MODES savedDislikeMode_ = DISLIKE_MODES::COUNT;  // value on flash





    std::vector<size_t> layers_nodes;

    const bool use_constant_weight_init = false;
    const float constant_weight_init = 0;

    SynthMLP synthMapping;  // value member -> lives in the (static) mode object: zero heap

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


    // Exploration-noise travel speed.
    static constexpr float kNoiseDt = 0.004f;      // normal OU travel speed (set in setup)

    bool resetMinMaxFlag = false;

    // Deferred actions: set from ISR, consumed in main-loop loopCallback before optimise()
    volatile bool pendingLike_{false};
    volatile bool pendingDislike_{false};
    volatile bool pendingDragStore_{false};   // drag-release: store savedAction
    volatile bool pendingInputSourceChange_{false};  // input-source change: deferred from rotary ISR
    INPUT_SOURCE pendingInputSource_{INPUT_SOURCE::JOYSTICK_3D};
    // Flash persistence is deferred until the selector view loses focus (encoder switch
    // pressed or navigated away), not done on a timer: a flash write disables interrupts
    // and pauses the other core long enough to blank the display, so it must not happen
    // while the user is still scrolling. Set from the rotary ISR, consumed in loopCallback.
    volatile bool pendingSettingsCommit_{false};

    spin_lock_t *mlpActive;

    // --- Train on demand (see setTrainOnDemand) --------------------------------------
    // Settle when the error on the likes is low, or has stopped improving (a small net
    // with conflicting/noisy likes has an error floor it can't train below). Wake when it
    // rises clearly above where it settled.
    static constexpr float kSettleError = 1e-3f;       // settle below this (MSE) outright,
    static constexpr int kSettleChecks = 3;            //   for this many checks in a row
    static constexpr float kPlateauGain = 0.01f;       // or: <1% better over...
    static constexpr uint32_t kPlateauWindowMs = 1000; // ...this long (training is slow, so
                                                       //  a short window misreads a plateau)
    static constexpr uint32_t kSettleEvalMs = 100;     // full-error check while training
    static constexpr float kWakeRatio = 1.5f;          // wake above settled error * this
    static constexpr float kWakeMargin = 5e-4f;        //   + this
    static constexpr uint32_t kWatchdogMs = 1000;      // full-error check while settled
    bool trainOnDemand_ = false;
    bool trainSettled_ = false;
    int settleGoodChecks_ = 0;
    float plateauRefError_ = 1e9f;  // error at the start of the plateau window
    uint32_t plateauRefMs_ = 0;
    float settledError_ = 0.f;      // error when it settled (wake reference)
    uint32_t lastErrorEvalMs_ = 0;
    std::vector<float> evalOut_;
    // Only sets state (safe from the button/ISR paths that reach it).
    void wakeTraining() {
        trainSettled_ = false;
        settleGoodChecks_ = 0;
        plateauRefError_ = 1e9f;
        plateauRefMs_ = millis();
        if (nnOutputsGraphView) nnOutputsGraphView->setTrainingIdle(false);
    }
    float likedError();          // mean MSE of the net over all liked memories (no update)
    void updateTrainState(bool trainedPositive, float lossPositive, bool pending);

    String _modeRoot{"mlp_rl"};
    String _modeTag{"Unknown"};
    ExtraSaveDataFn _extraSaveFn;
    ExtraLoadDataFn _extraLoadFn;

};

// Template method definitions (header-only so the per-mode instantiation is
// available wherever InterfaceRL<N> is used).
#include "InterfaceRL.tpp"

#endif // INTERFACERL_HPP