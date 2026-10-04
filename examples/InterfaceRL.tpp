#include <LittleFS.h>
#include "../utils/sharedMem.hpp" // Required for READ_VOLATILE, sharedMem constants and PERIODIC_DEBUG
#include <Arduino.h>     // Required for Serial, millis, delay
#include "../hardware/memlnaut/MEMLNaut.hpp" // Required for MEMLNaut::Instance()
// display.hpp is included via InterfaceRL.hpp

inline float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}



// Protected helper method implementations
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_perform_like_action() {
    static std::vector<String> likemsgs = {
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
    escalateStage_ = 0;
    lastDislikeInput_.clear();  // a like ends any run of 'no's
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("yes");
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_perform_dislike_action() {
    static std::vector<String> dislikemsgs = {
        "oh no!", "Get rid of this sound", 
        "Why even bother?", "New sound please!", "No, please no!!!",
        "Thumbs down", "I'm so sorry", "I'm trying my hardest...",
        "I'm doing the best I can", "Learning...",
	    "I'll try to do better.", "Still figuring things out.",
        "Thanks for the feedback.", "Working on it!", "Oops, my bad.",
        "Learning from this.", "I'll adjust, promise.", "Noted", "Rearranging",
        "Let's move on!"
    };
    if (dislikeMode_ != DISLIKE_MODES::CURRENT) {
        handleDislike();  // posts its own message
        if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("no");
        return;
    }
    String msg = dislikemsgs[rand() % dislikemsgs.size()];
    this->storeExperience(-1.f, controlInput, action);
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("no");
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_perform_randomiseRL_action() {

    this->randomiseTheNetwork();
    this->generateAction(true);
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("scramble");
    DEBUG_PRINTLN("Randomising networks");
    if (msgView) msgView->post("Scrambling the network");
}

// Public trigger methods — called from ISR context, so only set a flag.
// The actual action runs in the main-loop loopCallback before optimise().
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::trigger_like() {
    pendingLike_ = true;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::trigger_dislike() {
    pendingDislike_ = true;
}

// void InterfaceRL::trigger_randomiseRL() {
//     _perform_randomiseRL_action();
// }


template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::setOptimiseDivisorInterf(float value)
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


template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::bind_RL_interface(INPUT_MODES input_mode, bool joystick4D) {

    loadInputSource();
    configureInputsView();
    loadDislikeMode();

    // Set up momentary switch callbacks
    MEMLNaut::Instance()->setMomA1Callback([this]() {
        if (MEMLNaut::Instance()->getMOMA1State()) {
            this->trigger_like();
        }
    });
    MEMLNaut::Instance()->setMomA2Callback([this]() {
        if (MEMLNaut::Instance()->getMOMA2State()) {
            this->trigger_dislike();
        }
    });
    MEMLNaut::Instance()->setMomB1Callback([this]() {
        if (MEMLNaut::Instance()->getMOMB1State()) {
            _perform_randomiseRL_action();
        }
    });
    // B2 = jolt: plant new synthetic points (see seedJolt). The callback may run in
    // the button ISR, so just flag it for the loop callback.
    MEMLNaut::Instance()->setMomB2Callback([this]() {
        if (MEMLNaut::Instance()->getMOMB2State()) pendingSeed_ = true;
    });

    // Always register joystick callbacks — they write to raw_joystick_
    // (ignored by assembleInputs() when a non-joystick source is active)
    MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
        if (state) {
            savedAction = action;
            actionBeingDragged = true;
            if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drag");
            if (msgView) msgView->post("Where do you want it?");
        } else {
            if (actionBeingDragged) {
                actionBeingDragged = false;
                pendingDragStore_ = true;
                if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drop");
                if (msgView) msgView->post("Here!");
            }
        }
    });
    MEMLNaut::Instance()->setJoyXCallback([this](float value) { raw_joystick_[0] = value; newInput = true; });
    MEMLNaut::Instance()->setJoyYCallback([this](float value) { raw_joystick_[1] = value; newInput = true; });
    MEMLNaut::Instance()->setJoyZCallback([this](float value) { raw_joystick_[2] = value; newInput = true; });
    MEMLNaut::Instance()->setADC3Callback([this](float value) { raw_joystick_[3] = value; newInput = true; });


    MEMLNaut::Instance()->setTogB1Callback([this](bool state) { // scr_ref no longer captured directly
        if (state) {
            this->_forget_replay_mem_interf();
        }
    });

    MEMLNaut::Instance()->setTogA1Callback([this](bool state) {
        if (state) {
            savedAction = action;
            actionBeingDragged = true;
            if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drag");
            if (msgView) msgView->post("Where do you want it?");
        } else {
            if (actionBeingDragged) {
                actionBeingDragged = false;
                pendingDragStore_ = true;  // deferred: storeExperience in loopCallback
                if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drop");
                if (msgView) msgView->post("Here!");
            }
        }
    });


    MEMLNaut::Instance()->setRVX1Callback(
        rvX1Override ? rvX1Override : RVCallback([this](float value) { this->setRewardScaleInterf(value); }));

    MEMLNaut::Instance()->setRVY1Callback(
        rvY1Override ? rvY1Override : RVCallback([this](float value) { this->setLRScale(value); }));

    MEMLNaut::Instance()->setRVZ1Callback(
        rvZ1Override ? rvZ1Override : RVCallback([this](float value) { setNoiseLevel(value); }));
    // Set up loop callback
    MEMLNaut::Instance()->setLoopCallback([this]() {
        if (pendingSeed_) {  // jolt (B2)
            pendingSeed_ = false;
            seedJolt();
        }
        // Process deferred actions from ISR before touching replayMem in optimise
        if (pendingLike_) {
            pendingLike_ = false;
            _perform_like_action();
        }
        if (pendingDislike_) {
            pendingDislike_ = false;
            _perform_dislike_action();
        }
        if (pendingDragStore_) {
            pendingDragStore_ = false;
            this->storeExperience(1.f, controlInput, savedAction);
            escalateStage_ = 0;
            lastDislikeInput_.clear();  // a like ends any run of 'no's
            if (nnOutputsGraphView) {
                size_t pos = 0;
                for (size_t i = 0; i < replayMem.size(); i++)
                    if (replayMem.getItem(i).reward > 0.f) pos++;
                nnOutputsGraphView->setMemoryCounts(pos, replayMem.size() - pos);
            }
        }
        // Apply a deferred input-source change off the rotary ISR (heap/SPI/flash IO).
        // Apply in-memory now for a responsive UI; the flash write waits for the commit
        // below, since scrolling through sources would otherwise blank the display.
        if (pendingInputSourceChange_) {
            pendingInputSourceChange_ = false;
            setInputSource(pendingInputSource_, false);
        }
        // Dislike-mode change from the rotary ISR: same apply-now, save-on-commit pattern.
        if (pendingDislikeModeChange_) {
            pendingDislikeModeChange_ = false;
            setDislikeMode(pendingDislikeMode_, false);
        }
        // A selector lost focus: persist whatever changed (the save* calls skip no-ops).
        // Any pending change above was applied first, so the latest value is saved.
        if (pendingSettingsCommit_) {
            pendingSettingsCommit_ = false;
            saveInputSource();
            saveDislikeMode();
        }
        // Joystick map on the NN Inputs screen: refresh the memory dots a few times a
        // second, and only while that screen is showing.
        if (millis() - lastMemPointsMs_ >= 300) {
            lastMemPointsMs_ = millis();
            pushMemoryPointsToInputsView();
        }
        uint32_t save = spin_lock_blocking(mlpActive);
        this->optimiseSometimes();
        this->generateAction();
        spin_unlock(mlpActive, save);
    });
}


template<size_t N_OUTPUTS, size_t N_INPUTS>
float InterfaceRL<N_OUTPUTS, N_INPUTS>::likedError() {
    float sum = 0.f;
    size_t n = 0;
    evalOut_.resize(N_OUTPUTS);
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.reward <= 0.f) continue;
        synthMapping.GetOutput(item.input, &evalOut_);
        const size_t m = std::min(evalOut_.size(), item.action.size());
        float se = 0.f;
        for (size_t j = 0; j < m; j++) {
            const float d = item.action[j] - evalOut_[j];
            se += d * d;
        }
        sum += m ? se / static_cast<float>(m) : 0.f;  // same scale as TrainBatch's MSE
        n++;
    }
    for (const auto& sp : synth_) {  // seed-jolt points count as likes here
        synthMapping.GetOutput(sp.input, &evalOut_);
        const size_t m = std::min(evalOut_.size(), sp.target.size());
        float se = 0.f;
        for (size_t j = 0; j < m; j++) {
            const float d = sp.target[j] - evalOut_[j];
            se += d * d;
        }
        sum += m ? se / static_cast<float>(m) : 0.f;
        n++;
    }
    return n ? sum / static_cast<float>(n) : 0.f;
}

// Called after each training step: settle once the error has stayed low (checked on
// all likes, not just the noisy batch loss) and nothing time-limited is pending.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::updateTrainState(bool trainedPositive, float lossPositive, bool pending) {
    (void)trainedPositive; (void)lossPositive;
    const uint32_t t = millis();
    if (pending) {  // something time-limited still working: restart the settle tests
        settleGoodChecks_ = 0;
        plateauRefError_ = 1e9f;
        plateauRefMs_ = t;
        return;
    }
    if (t - lastErrorEvalMs_ < kSettleEvalMs) return;
    lastErrorEvalMs_ = t;
    const float e = likedError();

    // Low: settle after kSettleChecks consecutive low checks.
    settleGoodChecks_ = (e < kSettleError) ? settleGoodChecks_ + 1 : 0;
    bool settle = settleGoodChecks_ >= kSettleChecks;

    // Plateau: once per window, compare with the error a window ago.
    if (!settle && t - plateauRefMs_ >= kPlateauWindowMs) {
        settle = e > plateauRefError_ * (1.f - kPlateauGain);
        plateauRefError_ = e;
        plateauRefMs_ = t;
    }
    if (!settle) return;
    trainSettled_ = true;
    settledError_ = e;
    if (nnOutputsGraphView) nnOutputsGraphView->setTrainingIdle(true);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::setRewardScaleInterf(float value)
{
    this->setRewardScale(value);
    String msg = "Reward scale: " + String(value);
    if (msgView) msgView->post(msg);
}



template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_forget_replay_mem_interf()
{
    this->forgetMemory();
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("forget");
    static std::vector<String> forgetmsgs = {
        "Erasing my memory", "Forgetting everything", "Memory wiped","Thank you Susan?",
        "Starting afresh", "Why care about the past?","Living in the moment"
    };
    String msg = forgetmsgs[rand() % forgetmsgs.size()];

    if (msgView) msgView->post(msg);
}


template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf, bool enableFootcontroller)
{
    if (midi_interf) {
        midi_interf->SetCCCallback([this, enableFootcontroller] (uint8_t cc_number, uint8_t cc_value) {
            // Route CC1-CC8 to raw_midi_ when a MIDI input source is active
            bool is_midi_source = (input_source_ == INPUT_SOURCE::MIDI_1CC ||
                                   input_source_ == INPUT_SOURCE::MIDI_3CC ||
                                   input_source_ == INPUT_SOURCE::MIDI_8CC);
            if (is_midi_source && cc_number >= 1 && cc_number <= 8) {
                raw_midi_[cc_number - 1] = static_cast<float>(cc_value) / 127.f;
                newInput = true;
                return;
            }
            if (!enableFootcontroller) return;
            Serial.printf("MIDI CC %d: %d\n", cc_number, cc_value);
            switch(cc_number) {
                case 1:
                {
                    if (cc_value > 0) this->_perform_like_action();
                    break;
                }
                case 2:
                {
                    if (cc_value > 0) this->_perform_dislike_action();
                    break;
                }
                case 3:
                {
                    if (cc_value > 0) this->_perform_randomiseRL_action();
                    break;
                }
                case 4:
                {
                    if (cc_value > 0) this->_forget_replay_mem_interf();
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

    if (ccSelectView && !ccSelectView->getSelectedCCs().empty()) {
        midi_->SetParamCCNumbers(ccSelectView->getSelectedCCs());
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::setup(size_t n_inputs, size_t n_outputs, bool addMessageView)
{

    InterfaceBase::setup(n_inputs, n_outputs);

    mlpActive = spin_lock_init(spin_lock_claim_unused(true));

    // The compile-time network fixes n_outputs == N_OUTPUTS; the runtime arg
    // is the active mode's kN_Params, which equals N_OUTPUTS by construction.
    (void)n_outputs;

    layers_nodes = { n_inputs, 16, 16, n_outputs };

    controlInput.resize(layers_nodes[0]);
    action.resize(n_outputs, 0.5f);  // Initialize action vector with default values
    mappingOutput.resize(n_outputs);

    //init networks — StaticMLP is a value member (fixed arch, all weights in
    // the static mode object: no heap). Just initialise its weights.
    // synthMapping.InitXavier();
    synthMapping.RandomiseWeightsAndBiasesLin(-1.2f,0.9f, 0, 0.5);

    rewardScale = 1.0f; // Default reward scale

    // randomiseTheNetwork();

    // Memory limit
    replayMem.setMemoryLimit(memoryLimit);

    itemsToRemove.reserve(replayMem.getMemoryLimit());


    // GUI
    if (!nnOutputsGraphView) {
        nnOutputsGraphView = std::make_shared<RLView>("RL", n_outputs, 4, TFT_GREEN, 0.f, 1.f);
    }
    MEMLNaut::Instance()->disp->AddView(nnOutputsGraphView);
    nnInputsGraphView = std::make_shared<InputsView>("NN Inputs", n_inputs);
    MEMLNaut::Instance()->disp->AddView(nnInputsGraphView);

    {
        static String dislikeModeNames[] = {
            "Current (push away)", "Nudge + blame", "Re-roll", "Nudge>Back>Re-roll",
            "Borrow from likes"
        };
        dislikeModeView = std::make_shared<RotarySelectView>("Dislike Mode");
        dislikeModeView->setOptions(std::span<String>(dislikeModeNames,
            static_cast<size_t>(DISLIKE_MODES::COUNT)));
        static String dislikeModeDescs[] = {
            "Push the sound away from your likes",
            "Step away, mostly undoing what differs from the nearest like",
            "Jump to a fresh sound here",
            "Repeated no: nudge, then back to a like, then re-roll",
            "Rebuild from qualities of nearby likes"
        };
        dislikeModeView->setDescriptions(std::span<String>(dislikeModeDescs,
            static_cast<size_t>(DISLIKE_MODES::COUNT)));
        dislikeModeView->setSelection(static_cast<size_t>(dislikeMode_));
        dislikeModeView->setNewSelectionCallback([this](size_t idx) {
            // Runs in the rotary-encoder ISR — defer to the main loop.
            if (idx < static_cast<size_t>(DISLIKE_MODES::COUNT)) {
                pendingDislikeMode_ = static_cast<DISLIKE_MODES>(idx);
                pendingDislikeModeChange_ = true;
            }
        });
        dislikeModeView->setFocusLostCallback([this]() { pendingSettingsCommit_ = true; });
        MEMLNaut::Instance()->disp->AddView(dislikeModeView);
    }
    initExploreNoise();
    // memoryStoreModeView = std::make_shared<SingleSelectView>("Mem Mode");
    // MEMLNaut::Instance()->disp->AddView(memoryStoreModeView);
    // memoryStoreModeView->setOptions(memOptions);
    // memoryStoreModeView->setNewVoiceCallback([this](size_t idx) {
    //     memoryStoreMode = static_cast<MEMORY_STORE_MODES>(idx);
    // });

    if (addMessageView) {
        msgView = std::make_shared<MessageView>("Messages");
        MEMLNaut::Instance()->disp->AddView(msgView);
    }

    // 12 slots, 6 x 2. Slot style: numbered corners, empty slots dimmed; the slot of the
    // model currently in use is filled with the accent colour on both screens.
    const std::vector<String> emptySlots(kNumSlots, String(""));
    fileSaveView = std::make_shared<BlockSelectView>("Save Model", kSaveAccent, kNumSlots, 43, 78,
        TFT_WHITE, emptySlots, kSaveAccent);
    fileSaveView->setSlotStyle(true);
    fileSaveView->SetOnSelectCallback([this](size_t id) {
        pendingSaveSlot = static_cast<int>(id) - 1;
        // Unnamed saves are recorded under their number: don't offer that as a name.
        const String& cur = slotNames[pendingSaveSlot];
        nameInputView->reset(cur == String(pendingSaveSlot + 1) ? String("") : cur);
        MEMLNaut::Instance()->disp->ShowDialog(nameInputView);
    });
    MEMLNaut::Instance()->disp->AddView(fileSaveView);

    fileLoadView = std::make_shared<BlockSelectView>("Load Model", kLoadAccent, kNumSlots, 43, 78,
        TFT_WHITE, emptySlots, kLoadAccent);
    fileLoadView->setSlotStyle(true);
    fileLoadView->SetOnSelectCallback([this](size_t id) {
        int slotIdx = static_cast<int>(id) - 1;
        String filename = (slotNames[slotIdx].length() > 0) ? slotNames[slotIdx] : String(id);
        fileLoadView->SetMessage("Loading " + filename);
        uint32_t save = spin_lock_blocking(mlpActive);
        if (MEMLNaut::Instance()->startSD()) {
            if (this->_load_RL_from_SD(filename)) {
                synth_.clear();
                wakeTraining();  // new weights and memories
                fileLoadView->SetMessage("Loaded " + filename);
                markCurrentSlot(slotIdx);
            } else {
                fileLoadView->SetMessage("Failed to load model");
            }
            MEMLNaut::Instance()->stopSD();
        } else {
            fileLoadView->SetMessage("SD card error - is it inserted and formatted?");
        }
        spin_unlock(mlpActive, save);
    });
    MEMLNaut::Instance()->disp->AddView(fileLoadView);

    nameInputView = std::make_shared<NameInputView>("Name");
    nameInputView->setCallbacks(
        [this](const String& name) {
            if (pendingSaveSlot >= 0 && pendingSaveSlot < kNumSlots) {
                String displayName = (name.length() > 0) ? name : String(pendingSaveSlot + 1);
                // Record unnamed saves under their number too, so the slot shows as used
                // (the file is named the same way, so loading is unchanged).
                slotNames[pendingSaveSlot] = displayName;
                fileSaveView->updateButtonName(static_cast<size_t>(pendingSaveSlot), displayName);
                fileLoadView->updateButtonName(static_cast<size_t>(pendingSaveSlot), displayName);
                fileSaveView->SetMessage("Saving as " + displayName);
                uint32_t save = spin_lock_blocking(mlpActive);
                if (MEMLNaut::Instance()->startSD()) {
                    _saveSlotNames();
                    if (this->_save_RL_to_SD(displayName)) {
                        fileSaveView->SetMessage("Saved as " + displayName);
                        markCurrentSlot(pendingSaveSlot);
                    } else {
                        fileSaveView->SetMessage("Failed to save model");
                    }
                    MEMLNaut::Instance()->stopSD();
                } else {
                    fileSaveView->SetMessage("SD card error - is it inserted and formatted?");
                }
                spin_unlock(mlpActive, save);
            }
            MEMLNaut::Instance()->disp->DismissDialog();
        },
        [this]() {
            MEMLNaut::Instance()->disp->DismissDialog();
        }
    );
    MEMLNaut::Instance()->disp->RegisterDialog(nameInputView);
}


template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::setModeInfo(const String& modeRoot, const String& modeTag) {
    _modeRoot = modeRoot;
    _modeTag = modeTag;
    if (MEMLNaut::Instance()->startSD()) {
        _loadSlotNames();
        MEMLNaut::Instance()->stopSD();
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
bool InterfaceRL<N_OUTPUTS, N_INPUTS>::_save_RL_to_SD(String id) {
    String dir = "/" + _modeRoot;
    String path = dir + "/" + id + ".bin";

    if (!SD.exists(dir.c_str())) {
        SD.mkdir(dir.c_str());
    }

    auto file = SD.open(path.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing: " + path);
        return false;
    }
    file.seek(0);

    MEMLFileHeader header;
    memcpy(header.magic, "MEML", 4);
    header.format_version = MEML_FILE_FORMAT_VERSION;
    memset(header.mode_tag, 0, sizeof(header.mode_tag));
    strncpy(header.mode_tag, _modeTag.c_str(), sizeof(header.mode_tag) - 1);

    std::vector<uint8_t> extraData;
    if (_extraSaveFn) {
        extraData = _extraSaveFn();
    }
    header.extra_size = static_cast<uint16_t>(extraData.size());

    if (file.write((const char*)&header, sizeof(header)) != sizeof(header)) {
        file.close();
        return false;
    }
    if (!extraData.empty()) {
        if (file.write(extraData.data(), extraData.size()) != extraData.size()) {
            file.close();
            return false;
        }
    }

    bool success = synthMapping.SaveMLPNetworkToFile(file);
    file.close();
    return success;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
bool InterfaceRL<N_OUTPUTS, N_INPUTS>::_load_RL_from_SD(String id) {
    String path = "/" + _modeRoot + "/" + id + ".bin";

    auto file = SD.open(path.c_str(), FILE_READ);
    if (!file) {
        Serial.println("File not found: " + path);
        return false;
    }

    MEMLFileHeader header;
    if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
        file.close();
        Serial.println("File too small to contain header");
        return false;
    }
    if (memcmp(header.magic, "MEML", 4) != 0) {
        file.close();
        Serial.println("Unrecognised file format (bad magic)");
        return false;
    }
    if (header.format_version > MEML_FILE_FORMAT_VERSION) {
        file.close();
        Serial.println("File saved with newer firmware (version " + String(header.format_version) + ")");
        return false;
    }
    char expected_tag[17] = {};
    strncpy(expected_tag, _modeTag.c_str(), 16);
    if (memcmp(header.mode_tag, expected_tag, 16) != 0) {
        char tag_buf[17] = {};
        memcpy(tag_buf, header.mode_tag, 16);
        file.close();
        Serial.println(String("Wrong mode: file is for '") + tag_buf + "'");
        return false;
    }

    if (header.extra_size > 0) {
        std::vector<uint8_t> extraData(header.extra_size);
        if (file.read(extraData.data(), header.extra_size) != header.extra_size) {
            file.close();
            return false;
        }
        if (_extraLoadFn) {
            _extraLoadFn(extraData.data(), header.extra_size, header.format_version);
        }
    }

    bool success = synthMapping.LoadMLPNetworkFromFile(file);
    file.close();

    // With a StaticMLP the architecture is fixed at compile time and
    // LoadMLPNetworkFromFile already rejects (returns false) any on-card model
    // whose geometry/activations don't match — so a loaded model is always
    // architecture-correct. Keep a defensive rebuild for the mismatch case.
    if (success && (synthMapping.get_num_inputs()  != (int)controlInput.size()
                 || synthMapping.get_num_outputs() != (int)n_outputs_)) {
        synthMapping.RandomiseWeightsAndBiasesLin(-1.2f, 0.9f, 0, 0.5f);
        if (msgView) msgView->post("Model incompatible: wrong architecture");
        return false;
    }
    return success;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_saveSlotNames() {
    String dir = "/" + _modeRoot;
    if (!SD.exists(dir.c_str())) {
        SD.mkdir(dir.c_str());
    }
    String path = dir + "/slots.txt";
    auto file = SD.open(path.c_str(), FILE_WRITE);
    if (!file) return;
    file.seek(0);
    for (int i = 0; i < kNumSlots; i++) {
        file.println(slotNames[i]);
    }
    file.close();
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::_loadSlotNames() {
    String path = "/" + _modeRoot + "/slots.txt";
    auto file = SD.open(path.c_str(), FILE_READ);
    if (!file) return;
    for (int i = 0; i < kNumSlots; i++) {
        String line = file.readStringUntil('\n');
        line.trim();
        slotNames[i] = line;
        if (line.length() > 0) {
            fileSaveView->updateButtonName(static_cast<size_t>(i), line);
            fileLoadView->updateButtonName(static_cast<size_t>(i), line);
        }
    }
    file.close();
}


template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::optimise() {

    // Train on demand: settled -> no training; just an occasional watchdog check.
    if (trainOnDemand_ && trainSettled_) {
        const uint32_t t = millis();
        if (t - lastErrorEvalMs_ < kWatchdogMs) return;
        lastErrorEvalMs_ = t;
        const float e = likedError();
        if (e <= settledError_ * kWakeRatio + kWakeMargin) return;
        wakeTraining();
    }

    float lossPositive{0.f};
    bool trainedPositive = false;
    float lossNegative{0.f};
    size_t batchSizeNeg=0;
    const float effLR = learningRateScaled;

    //positive batch
    std::vector<size_t> sample = replayMem.sampleIndices(batchSize);
    if (sample.size() >1) {
        //run sample through network
        size_t batchSizePos=0;
        float avgRewardPos=0.f;
        training_pair_t tsPositive;

        // Pre-allocate to avoid repeated allocations
        tsPositive.first.reserve(sample.size());
        tsPositive.second.reserve(sample.size());


        // Positive batch: random sample (diversity for generalisation)
        for (auto &i : sample) {
            if (replayMem.getItem(i).reward > 0) {
                tsPositive.first.push_back(replayMem.getItem(i).input);
                tsPositive.second.push_back(replayMem.getItem(i).action);
                batchSizePos++;
                avgRewardPos += replayMem.getItem(i).reward;
            }
        }

        if (batchSizePos > 0){
            avgRewardPos /= static_cast<float>(batchSizePos);
            lossPositive = synthMapping.TrainBatch(tsPositive, effLR * avgRewardPos, 1, batchSize, 0.f, false);
            trainedPositive = true;
            // Serial.printf("[DEBUG] Loss after positive TrainBatch: %f (inf=%d, nan=%d)\n",
            //              lossPositive, std::isinf(lossPositive), std::isnan(lossPositive));
        }
    }

    // Negative batch: scan ALL negatives so every dislike is guaranteed to push.
    // No decay — a 'no' pushes at full strength until it's lived kDislikeLifetimeMs,
    // then it's removed outright.
    training_pair_t tsNegative;
    tsNegative.first.reserve(sample.size());
    tsNegative.second.reserve(sample.size());
    // Single scan over all memory: tally positives (for display + LR ratio) and collect
    // negatives (expiring any that have outlived kDislikeLifetimeMs).
    float avgRewardNeg=0.f;
    size_t totalPosCount=0;
    const uint32_t now = millis();
    for (size_t i = 0; i < replayMem.size(); i++) {
        float reward = replayMem.getItem(i).reward;
        if (reward > 0.f) { totalPosCount++; continue; }
        if ((now - static_cast<uint32_t>(replayMem.getTimestamp(i))) >= kDislikeLifetimeMs) {
            itemsToRemove.push_back(i);  // lived its lifetime -> stop pushing, remove
            continue;
        }
        tsNegative.first.push_back(replayMem.getItem(i).input);
        tsNegative.second.push_back(replayMem.getItem(i).action);
        batchSizeNeg++;
        avgRewardNeg += reward;
    }
    if (batchSizeNeg > 0){

        struct PosCandidate { float dist; size_t idx; };
        std::vector<PosCandidate> candidates;
        candidates.reserve(replayMem.size());
        for (size_t i = 0; i < replayMem.size(); i++) {
            const auto& item = replayMem.getItem(i);
            if (item.reward > 0.f)
                candidates.push_back({euclideanDistance(item.input, controlInput), i});
        }
        std::sort(candidates.begin(), candidates.end(),
                [](const PosCandidate& a, const PosCandidate& b){ return a.dist < b.dist; });

        std::vector<float> meanPositiveAction(action.size(), 0.f);
        size_t posMemCount = 0;
        const size_t kUsed = std::min(candidates.size(), kCentroidK);
        for (size_t ci = 0; ci < kUsed; ci++) {
            const auto& item = replayMem.getItem(candidates[ci].idx);
            for (size_t j = 0; j < meanPositiveAction.size(); j++)
                meanPositiveAction[j] += item.action[j];
            posMemCount++;
        }
        if (posMemCount > 0) {
            for (auto& v : meanPositiveAction) v /= static_cast<float>(posMemCount);
        }        
        avgRewardNeg /= static_cast<float>(batchSizeNeg);

        // Push each disliked action's training target strongly away from the liked
        // centroid — or in a random direction when there are no likes yet. No taper:
        // a 'no' should clearly move the mapping away even from a sound already far
        // from the liked region (the taper used to kill exactly that case). Bigger
        // kGeometricPushScale + higher negLRRatio => the sound slides away faster/further.
        const bool havePositives = (posMemCount > 0);
        training_pair_t tsGeometric;
        tsGeometric.first = tsNegative.first;
        tsGeometric.second.reserve(tsNegative.second.size());

        float pushStep = std::clamp(fabsf(avgRewardNeg), 0.25f, 1.0f) * kGeometricPushScale;

        for (const auto& neg_action : tsNegative.second) {
            // Fix 3: guard against size mismatch with old saved actions
            const size_t dimCount = std::min(neg_action.size(), meanPositiveAction.size());
            float len = 0.f;
            std::vector<float> dir(dimCount);
            for (size_t j = 0; j < dimCount; j++) {
                dir[j] = neg_action[j] - meanPositiveAction[j];  // meanPositiveAction is 0 when no likes
                len += dir[j] * dir[j];
            }
            len = sqrtf(len);
            const bool useRandom = !havePositives || (len <= 1e-4f);
            std::vector<float> target(neg_action);  // copy keeps out-of-range dims intact
            for (size_t j = 0; j < dimCount; j++) {
                bool active = activeDims_.empty() || (j < activeDims_.size() && activeDims_[j]);
                if (!active) continue;
                float d = useRandom
                    ? (static_cast<float>(rand() & 0xFF) / 127.5f - 1.f)
                    : (dir[j] / len);
                target[j] = std::clamp(neg_action[j] + d * pushStep, 0.f, 1.f);
            }
            tsGeometric.second.push_back(std::move(target));
        }
        // Dynamic LR ratio: push harder when dislikes are rare, gentler when they flood the buffer
        const float negFraction = static_cast<float>(batchSizeNeg)
            / static_cast<float>(std::max(batchSizeNeg + totalPosCount, size_t{1}));
        const float negLRRatio = kNegLRBase - 0.4f * negFraction;
        lossNegative = synthMapping.TrainBatch(tsGeometric, effLR * negLRRatio, 1, batchSizeNeg, 0.f, false);
    }

    // Fix 4: always clear — stale indices corrupt subsequent optimise() calls
    replayMem.removeItems(itemsToRemove);
    itemsToRemove.clear();

    // Non-CURRENT dislike modes: bounded, self-retiring targets (none in CURRENT mode).
    trainDislikeTargets(effLR * kDislikeLR);

    // Jolt: train towards the synthetic points (strongly just after a jolt).
    const bool synthPending = trainSynth(effLR);

    if (trainOnDemand_) {
        // Anything time-limited still working keeps training on.
        const bool pending = batchSizeNeg > 0 || !dislikeTargets_.empty() || synthPending;
        updateTrainState(trainedPositive, lossPositive, pending);
    }

    if (nnOutputsGraphView) {
        // Only when a positive batch actually trained: lossPositive is 0 both for "no
        // likes to train on" and for a net that already reproduces every liked sound.
        if (trainedPositive) nnOutputsGraphView->setLoss(lossPositive);
        nnOutputsGraphView->setMemoryCounts(totalPosCount,
            replayMem.size() - totalPosCount + dislikeTargets_.size());
    }

}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::readAnalysisParameters(std::vector<float> params) {
    for (size_t i = 0; i < params.size() && i < 6; i++) {
        raw_ml_[i] = params[i];
    }
    generateAction(true);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::assembleInputs() {
    switch (input_source_) {
        case INPUT_SOURCE::JOYSTICK_3D:       copyAndZero(raw_joystick_, 3); break;
        case INPUT_SOURCE::JOYSTICK_4D:       copyAndZero(raw_joystick_, 4); break;
        case INPUT_SOURCE::MACHINE_LISTENING: copyAndZero(raw_ml_,       6); break;
        case INPUT_SOURCE::MIDI_1CC:          copyAndZero(raw_midi_,     1); break;
        case INPUT_SOURCE::MIDI_3CC:          copyAndZero(raw_midi_,     3); break;
        case INPUT_SOURCE::MIDI_8CC:          copyAndZero(raw_midi_,     8); break;
        case INPUT_SOURCE::COMBINED:
            memcpy(&controlInput[0], raw_joystick_, 4 * sizeof(float));
            memcpy(&controlInput[4], raw_ml_,       6 * sizeof(float));
            break;
        default: break;
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::copyAndZero(const float* src, size_t n) {
    // Pad the unused input tail with a non-zero constant instead of 0. A constant input
    // only adds a fixed term (Σ_j W1[i,j]·c) to each hidden unit — i.e. a per-unit layer-1
    // bias shift — which spreads effective biases to mixed signs so units switch both on
    // and off across a single-input sweep (more non-linear, direction-changing mapping).
    // unusedInputDefault_ is recomputed only on input-mode change (see updateUnusedInputDefault).
    size_t i = 0;
    for (; i < n && i < N_INPUTS; ++i) controlInput[i] = src[i];
    for (; i < N_INPUTS; ++i)          controlInput[i] = unusedInputDefault_;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::saveInputSource() {
    if (input_source_ == savedInputSource_) return;  // unchanged: skip the flash write
    FILE* f = fopen(kInputSourceFile, "wb");
    if (f) {
        fwrite(&input_source_, sizeof(input_source_), 1, f);
        fclose(f);
        savedInputSource_ = input_source_;
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::loadInputSource() {
    FILE* f = fopen(kInputSourceFile, "rb");
    if (f) {
        if (fread(&input_source_, sizeof(input_source_), 1, f) == 1)
            savedInputSource_ = input_source_;
        fclose(f);
    }
    updateUnusedInputDefault();
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::addInputSourceView(bool includeCCSelect) {
    configureInputsView();  // ML labels may have been set since bind (by the audio setup)

    std::vector<INPUT_SOURCE> available = {
        INPUT_SOURCE::JOYSTICK_3D, INPUT_SOURCE::JOYSTICK_4D,
        INPUT_SOURCE::MIDI_1CC, INPUT_SOURCE::MIDI_3CC, INPUT_SOURCE::MIDI_8CC
    };
    if (hasMachineListening_) {
        available.push_back(INPUT_SOURCE::MACHINE_LISTENING);
        available.push_back(INPUT_SOURCE::COMBINED);
    }

    auto describe = [](INPUT_SOURCE src) -> const char* {
        switch (src) {
            case INPUT_SOURCE::JOYSTICK_3D:       return "Joystick X, Y and twist";
            case INPUT_SOURCE::JOYSTICK_4D:       return "Both sticks: X, Y, Z and W";
            case INPUT_SOURCE::MACHINE_LISTENING: return "Audio input features: pitch, energy, brightness...";
            case INPUT_SOURCE::MIDI_1CC:          return "MIDI CC1 (mod wheel)";
            case INPUT_SOURCE::MIDI_3CC:          return "MIDI CC1-3";
            case INPUT_SOURCE::MIDI_8CC:          return "MIDI CC1-8";
            case INPUT_SOURCE::COMBINED:          return "Joystick plus audio features";
            default:                              return "";
        }
    };
    std::vector<String> opts, descs;
    for (auto src : available) {
        opts.push_back(inputSourceName(src));
        descs.push_back(describe(src));
    }

    size_t initialSel = 0;
    auto it = std::find(available.begin(), available.end(), input_source_);
    if (it != available.end()) initialSel = std::distance(available.begin(), it);

    auto view = std::make_shared<RotarySelectView>("Input Source");
    view->setOptions(std::span<String>(opts.data(), opts.size()));
    view->setDescriptions(std::span<String>(descs.data(), descs.size()));
    view->setSelection(initialSel);
    view->setNewSelectionCallback([this, available](size_t idx) {
        // Runs in the rotary-encoder ISR — defer the actual switch to the main loop.
        if (idx < available.size()) requestInputSource(available[idx]);
    });
    view->setFocusLostCallback([this]() { pendingSettingsCommit_ = true; });
    MEMLNaut::Instance()->disp->AddView(view);

    if (includeCCSelect) {
        size_t maxCC = midi_ ? midi_->getParamCount() : n_outputs_;
        ccSelectView = std::make_shared<CCSelectView>(maxCC, "MIDI CC Out");
        loadCCNumbers();
        ccSelectView->setOnChangeCallback([this](const std::vector<uint8_t>& ccs) {
            if (midi_) midi_->SetParamCCNumbers(ccs);
            saveCCNumbers();
        });
        MEMLNaut::Instance()->disp->AddView(ccSelectView);
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::generateAction(bool donthesitate) {
    // While noise is on, regenerate every cycle so it keeps moving even when the input is
    // still and training has settled (nothing else sets newInput then).
    if (newInput || donthesitate || noiseAmp_ > 0.f) {
        newInput = false;

        assembleInputs();
        if (inputInjectionHook) inputInjectionHook(controlInput);

        if (!actionBeingDragged) {
            if (noiseAmp_ > 0.f) noisyForward(mappingOutput);  // exploration: weight noise
            else synthMapping.GetOutput(controlInput, &mappingOutput);
        }
        if (paramTransformHook) paramTransformHook(mappingOutput);
        SendParamsToQueue(mappingOutput);
        action = mappingOutput;
        nnOutputsGraphView->UpdateValues(mappingOutput, resetMinMaxFlag);
        resetMinMaxFlag = false;
        nnInputsGraphView->UpdateValues(controlInput);
    }
}

// void InterfaceRL::storeExperience(float reward) {
//     std::vector<float> state = controlInput; 
//     trainStatelessRLItem trainItem = {state, action, reward}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
//     replayMem.add(trainItem, millis());
// }


template<size_t N_OUTPUTS, size_t N_INPUTS>
bool InterfaceRL<N_OUTPUTS, N_INPUTS>::removeItemsAtDistance(std::vector<float> &experienceState, const float distThreshold, const float reward) {
    std::vector<size_t> indicesToRemove;
    bool accumulated = false;
    for(size_t i=0; i < replayMem.size(); i++) {
        trainStatelessRLItem& item = replayMem.getItem(i);
        float dist = euclideanDistance(item.input, experienceState);
        if (dist < distThreshold) {
            if (reward < 0.f && item.reward < 0.f) {
                // Strengthen existing dislike rather than replacing it
                item.reward = std::max(item.reward + reward, -1.0f);
                accumulated = true;
            } else if (reward < 0.f && item.reward > 0.f) {
                // A dislike near a like: delete the like so it stops pulling the
                // model back towards the disliked region.
                indicesToRemove.push_back(i);
                if (msgView) msgView->post("Removing nearby like");
            } else if (item.reward > 0.f && reward > 0.f) {
                indicesToRemove.push_back(i);
                if (msgView) msgView->post("Removing similar memory item");
            }
        }
    }
    replayMem.removeItems(indicesToRemove);
    return accumulated;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::decayItemsAtDistance(std::vector<float> &experienceState, const float distThreshold) {
    std::vector<size_t> indicesToRemove;
    for(size_t i=0; i < replayMem.size(); i++) {
        trainStatelessRLItem& item = replayMem.getItem(i);   
        float dist = euclideanDistance(item.input, experienceState);
        if (dist < distThreshold) { 
            float decayFactor = (dist/distThreshold);
            item.reward *= decayFactor; // Decay reward 
            if (item.reward < 0.05f) {
                indicesToRemove.push_back(i); 
            }
            if (msgView) msgView->post("Decaying memory item");
            Serial.printf("Decayed item %d reward to %f\n", i, item.reward);
        }
    }
    replayMem.removeItems(indicesToRemove);             
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::storeExperience(float reward, std::vector<float> &experienceState, std::vector<float> &experienceAction ) {
    wakeTraining();  // new data
    trainStatelessRLItem trainItem = {experienceState, experienceAction, reward * rewardScale}; // state is s_t, action is a_t, reward is r_t, nextState is s_t
    bool skip_add = false;
    switch(memoryStoreMode) {
        case MEMORY_STORE_MODES::ADD:
            break;
        case MEMORY_STORE_MODES::REPLACE_5_PERCENT:
            skip_add = removeItemsAtDistance(experienceState, 0.05f, trainItem.reward);
            break;
        case MEMORY_STORE_MODES::REPLACE_10_PERCENT:
            skip_add = removeItemsAtDistance(experienceState, 0.10f, trainItem.reward);
            break;
        case MEMORY_STORE_MODES::REPLACE_15_PERCENT:
            skip_add = removeItemsAtDistance(experienceState, 0.15f, trainItem.reward);
            break;
        case MEMORY_STORE_MODES::REWARD_DECAY_10_PERCENT:
            decayItemsAtDistance(experienceState, 0.10f);
            break;
        case MEMORY_STORE_MODES::REWARD_DECAY_20_PERCENT:
            decayItemsAtDistance(experienceState, 0.20f);
            break;
    }
    if (!skip_add) replayMem.add(trainItem, millis());
    if (nnOutputsGraphView) {
        size_t pos = 0;
        for (size_t i = 0; i < replayMem.size(); i++)
            if (replayMem.getItem(i).reward > 0.f) pos++;
        nnOutputsGraphView->setMemoryCounts(pos, replayMem.size() - pos);
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::saveCCNumbers() {
    if (!ccSelectView) return;
    String path = "/" + _modeRoot + "_cc_numbers.bin";
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        const auto& ccs = ccSelectView->getSelectedCCs();
        fwrite(ccs.data(), 1, ccs.size(), f);
        fclose(f);
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::loadCCNumbers() {
    if (!ccSelectView) return;
    String path = "/" + _modeRoot + "_cc_numbers.bin";
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        std::vector<uint8_t> ccs;
        uint8_t b;
        while (fread(&b, 1, 1, f) == 1) ccs.push_back(b);
        fclose(f);
        if (!ccs.empty()) {
            ccSelectView->setSelectedCCs(ccs);
            return;
        }
    }
    // Default: CC1..n_outputs
    size_t nDefault = std::min(ccSelectView->getMaxActive(), (size_t)32);
    std::vector<uint8_t> defaults(nDefault);
    for (size_t i = 0; i < nDefault; i++) defaults[i] = static_cast<uint8_t>(i + 1);
    ccSelectView->setSelectedCCs(defaults);
}


// ─── Dislike modes ──────────────────────────────────────────────────────────────────

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::setDislikeMode(DISLIKE_MODES m, bool persist) {
    static const char* names[] = { "push away", "nudge + blame", "re-roll", "nudge>back>re-roll",
                                   "borrow from likes" };
    dislikeMode_ = m;
    dislikeTargets_.clear();  // targets from the previous mode would keep training
    escalateStage_ = 0;
    lastDislikeInput_.clear();
    if (dislikeModeView) dislikeModeView->setSelection(static_cast<size_t>(m));
    if (msgView) msgView->post(String("Dislike: ") + names[static_cast<size_t>(m)]);
    if (persist) saveDislikeMode();
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::saveDislikeMode() {
    if (dislikeMode_ == savedDislikeMode_) return;  // unchanged: skip the flash write
    FILE* f = fopen(kDislikeModeFile, "wb");
    if (f) {
        fwrite(&dislikeMode_, sizeof(dislikeMode_), 1, f);
        fclose(f);
        savedDislikeMode_ = dislikeMode_;
    }
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::loadDislikeMode() {
    FILE* f = fopen(kDislikeModeFile, "rb");
    if (!f) return;
    uint8_t v = 0;
    if (fread(&v, 1, 1, f) == 1 && v < static_cast<uint8_t>(DISLIKE_MODES::COUNT)) {
        dislikeMode_ = static_cast<DISLIKE_MODES>(v);
        savedDislikeMode_ = dislikeMode_;
        if (dislikeModeView) dislikeModeView->setSelection(v);
    }
    fclose(f);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
float InterfaceRL<N_OUTPUTS, N_INPUTS>::rmsDistActive(const std::vector<float>& a, const std::vector<float>& b) const {
    const size_t n = std::min(a.size(), b.size());
    float sum = 0.f;
    size_t count = 0;
    for (size_t j = 0; j < n; j++) {
        if (!isActiveDim(j)) continue;
        const float d = a[j] - b[j];
        sum += d * d;
        count++;
    }
    return count ? sqrtf(sum / static_cast<float>(count)) : 0.f;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
int InterfaceRL<N_OUTPUTS, N_INPUTS>::nearestLikeIndex(const std::vector<float>& input) const {
    int best = -1;
    float bestDist = 0.f;
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.reward <= 0.f) continue;
        const float d = euclideanDistance(item.input, input);
        if (best < 0 || d < bestDist) { best = static_cast<int>(i); bestDist = d; }
    }
    return best;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
std::vector<float> InterfaceRL<N_OUTPUTS, N_INPUTS>::randomUnitDir(size_t n) const {
    std::vector<float> dir(n, 0.f);
    for (size_t j = 0; j < n; j++)
        if (isActiveDim(j)) dir[j] = static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f;
    return dir;  // normalised by stepTarget
}

// origin + dir scaled to an RMS displacement of rmsStep over the active dims. A component
// that would leave [kTargetLo, kTargetHi] is reflected back inside rather than clipped, so a
// sound already at an edge moves inward instead of piling up against it.
template<size_t N_OUTPUTS, size_t N_INPUTS>
std::vector<float> InterfaceRL<N_OUTPUTS, N_INPUTS>::stepTarget(const std::vector<float>& origin,
                                                                std::vector<float> dir, float rmsStep) const {
    float len = 0.f;
    size_t count = 0;
    for (size_t j = 0; j < dir.size(); j++) {
        if (!isActiveDim(j)) { dir[j] = 0.f; continue; }
        len += dir[j] * dir[j];
        count++;
    }
    len = sqrtf(len);
    if (len < 1e-6f || count == 0) return origin;
    const float scale = rmsStep * sqrtf(static_cast<float>(count)) / len;
    std::vector<float> target(origin);
    for (size_t j = 0; j < target.size() && j < dir.size(); j++) {
        if (!isActiveDim(j)) continue;
        float v = origin[j] + dir[j] * scale;
        if (v > kTargetHi) v = kTargetHi - (v - kTargetHi);
        if (v < kTargetLo) v = kTargetLo + (kTargetLo - v);
        target[j] = std::clamp(v, kTargetLo, kTargetHi);
    }
    return target;
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::addDislikeTarget(DislikeTarget&& t) {
    wakeTraining();
    // A new 'no' here supersedes older ones here, so successive presses don't fight.
    for (size_t i = dislikeTargets_.size(); i-- > 0;) {
        if (euclideanDistance(dislikeTargets_[i].input, t.input) < kDislikeInputRadius)
            dislikeTargets_.erase(dislikeTargets_.begin() + i);
    }
    if (dislikeTargets_.size() >= kMaxDislikeTargets) dislikeTargets_.erase(dislikeTargets_.begin());
    dislikeTargets_.push_back(std::move(t));
}

// Likes at this input whose sound is (nearly) the one just disliked: the player has
// changed their mind about them, so they go. Likes of a different sound here are kept —
// they're what the nudge/retreat steers towards.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::removeEndorsingLikes() {
    std::vector<size_t> idx;
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.reward > 0.f
            && euclideanDistance(item.input, controlInput) < kDislikeInputRadius
            && rmsDistActive(item.action, action) < kRepelMargin)
            idx.push_back(i);
    }
    if (!idx.empty()) {
        replayMem.removeItems(idx);
        if (msgView) msgView->post("Removing nearby like");
    }
}

// A: move a bounded distance away. With blame (E), the direction reverts the params that
// differ most from the nearest like (weighted by the squared difference); without it, the
// direction is away from the centroid of the nearest likes. No likes -> a random direction,
// fixed for this press.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::dislikeNudge(bool blame) {
    removeEndorsingLikes();
    const size_t n = action.size();
    std::vector<float> dir(n, 0.f);
    bool haveDir = false;
    String msg = "No: nudging away";

    if (blame) {
        const int li = nearestLikeIndex(controlInput);
        if (li >= 0) {
            const auto& ref = replayMem.getItem(static_cast<size_t>(li)).action;
            for (size_t j = 0; j < n && j < ref.size(); j++) {
                const float diff = ref[j] - action[j];
                dir[j] = diff * fabsf(diff);
            }
            haveDir = true;
            msg = "No: reverting what changed";
        }
    } else {
        struct Cand { float dist; size_t idx; };
        std::vector<Cand> cands;
        for (size_t i = 0; i < replayMem.size(); i++) {
            const auto& item = replayMem.getItem(i);
            if (item.reward > 0.f) cands.push_back({euclideanDistance(item.input, controlInput), i});
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.dist < b.dist; });
        const size_t k = std::min(cands.size(), kCentroidK);
        if (k > 0) {
            std::vector<float> centroid(n, 0.f);
            for (size_t c = 0; c < k; c++) {
                const auto& a = replayMem.getItem(cands[c].idx).action;
                for (size_t j = 0; j < n && j < a.size(); j++) centroid[j] += a[j];
            }
            for (size_t j = 0; j < n; j++) dir[j] = action[j] - centroid[j] / static_cast<float>(k);
            haveDir = true;
        }
    }

    float len = 0.f;
    for (size_t j = 0; j < n; j++) if (isActiveDim(j)) len += dir[j] * dir[j];
    if (!haveDir || len < 1e-8f) dir = randomUnitDir(n);

    addDislikeTarget({controlInput, action, stepTarget(action, std::move(dir), kRepelStep),
                      millis(), DislikeTarget::Kind::REPEL});
    if (msgView) msgView->post(msg);
}

// B: go back to the nearest liked sound (slightly jittered so it isn't an exact replay).
// Falls through to a re-roll if there's nothing to go back to.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::dislikeRetreat() {
    removeEndorsingLikes();
    const int li = nearestLikeIndex(controlInput);
    if (li < 0) { dislikeReroll(); return; }
    const auto& ref = replayMem.getItem(static_cast<size_t>(li)).action;
    std::vector<float> target(action);
    for (size_t j = 0; j < target.size() && j < ref.size(); j++) {
        if (!isActiveDim(j)) continue;
        const float jitter = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f) * kRetreatJitter;
        target[j] = std::clamp(ref[j] + jitter, kTargetLo, kTargetHi);
    }
    if (rmsDistActive(target, action) < kRepelMargin) { dislikeReroll(); return; }  // already there
    addDislikeTarget({controlInput, action, std::move(target), millis(), DislikeTarget::Kind::SEEK});
    if (msgView) msgView->post("No again: going back");
}

// C: a fresh sound for this input — half the time near a random liked sound, otherwise
// anywhere in the middle of the range — kept at least kRerollMinDist from the disliked one.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::dislikeReroll() {
    removeEndorsingLikes();
    std::vector<size_t> likes;
    for (size_t i = 0; i < replayMem.size(); i++)
        if (replayMem.getItem(i).reward > 0.f) likes.push_back(i);
    auto uni = []() { return static_cast<float>(rand()) / RAND_MAX; };

    std::vector<float> best;
    float bestDist = -1.f;
    for (int attempt = 0; attempt < 8 && bestDist < kRerollMinDist; attempt++) {
        std::vector<float> cand(action);
        const bool nearLike = !likes.empty() && (rand() & 1);
        const std::vector<float>* base = nearLike
            ? &replayMem.getItem(likes[rand() % likes.size()]).action : nullptr;
        for (size_t j = 0; j < cand.size(); j++) {
            if (!isActiveDim(j)) continue;
            float v;
            if (base && j < base->size()) {
                const float g = (uni() + uni() + uni() - 1.5f) * 2.f;  // ~N(0,1)
                v = (*base)[j] + g * kRerollSpread;
            } else {
                v = 0.15f + 0.7f * uni();
            }
            cand[j] = std::clamp(v, kTargetLo, kTargetHi);
        }
        const float d = rmsDistActive(cand, action);
        if (d > bestDist) { bestDist = d; best = std::move(cand); }
    }
    addDislikeTarget({controlInput, action, std::move(best), millis(), DislikeTarget::Kind::SEEK});
    if (msgView) msgView->post("No: trying something new");
}

// BORROW: the sound here moves towards the liked sounds nearby. Each active param either
// keeps its current value (a random kBorrowKeepFraction of them, so the sound retains some
// arbitrary character of its own) or is copied from one nearby like, chosen per param with
// probability weighted by how close that like's input is. Copying per param from different
// likes recombines their qualities rather than averaging them into a blur. Every target
// value already exists in a liked sound or the current one, so it can't run to the edges,
// and each press draws a fresh mix. No likes -> re-roll.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::dislikeBorrow() {
    removeEndorsingLikes();
    std::vector<size_t> likes;
    std::vector<float> w;
    float minD2 = 0.f;
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.reward <= 0.f) continue;
        const float d = euclideanDistance(item.input, controlInput);
        const float d2 = d * d;
        if (likes.empty() || d2 < minD2) minD2 = d2;
        likes.push_back(i);
        w.push_back(d2);
    }
    if (likes.empty()) { dislikeReroll(); return; }
    // Gaussian kernel on input distance, relative to the nearest like so it can't underflow
    // when every like is far away (then the nearest ones still dominate).
    const float inv2s2 = 1.f / (kBorrowInputSigma * kBorrowInputSigma);
    float wSum = 0.f;
    for (auto& x : w) { x = expf(-(x - minD2) * inv2s2); wSum += x; }
    auto uni = []() { return static_cast<float>(rand()) / RAND_MAX; };
    auto pickLike = [&]() -> const std::vector<float>& {
        float r = uni() * wSum;
        for (size_t k = 0; k < likes.size(); k++) {
            r -= w[k];
            if (r <= 0.f) return replayMem.getItem(likes[k]).action;
        }
        return replayMem.getItem(likes.back()).action;
    };

    // Keep fewer of the sound's own params on each retry until it has moved far enough.
    std::vector<float> best;
    float bestDist = -1.f;
    for (float keep = kBorrowKeepFraction; keep >= -0.01f && bestDist < kRepelMargin; keep -= 0.1f) {
        std::vector<float> cand(action);
        for (size_t j = 0; j < cand.size(); j++) {
            if (!isActiveDim(j) || uni() < keep) continue;
            const auto& donor = pickLike();
            if (j >= donor.size()) continue;
            const float jitter = (uni() * 2.f - 1.f) * kBorrowJitter;
            cand[j] = std::clamp(donor[j] + jitter, kTargetLo, kTargetHi);
        }
        const float d = rmsDistActive(cand, action);
        if (d > bestDist) { bestDist = d; best = std::move(cand); }
    }
    // The nearby likes all sound like this one: borrowing can't move it, so try something new.
    if (bestDist < kRepelMargin) { dislikeReroll(); return; }
    addDislikeTarget({controlInput, action, std::move(best), millis(), DislikeTarget::Kind::SEEK});
    if (msgView) msgView->post("No: borrowing from likes");
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::handleDislike() {
    switch (dislikeMode_) {
        case DISLIKE_MODES::BOUNDED_BLAME: dislikeNudge(true); break;
        case DISLIKE_MODES::REROLL:        dislikeReroll();    break;
        case DISLIKE_MODES::BORROW:        dislikeBorrow();    break;
        case DISLIKE_MODES::ESCALATE: {
            const uint32_t now = millis();
            const bool continuing = lastDislikeInput_.size() == controlInput.size()
                && (now - lastDislikeMs_) < kEscalateWindowMs
                && euclideanDistance(lastDislikeInput_, controlInput) < kEscalateInputRadius;
            escalateStage_ = continuing ? std::min<size_t>(escalateStage_ + 1, 2) : 0;
            lastDislikeMs_ = now;
            lastDislikeInput_ = controlInput;
            if (escalateStage_ == 0)      dislikeNudge(false);
            else if (escalateStage_ == 1) dislikeRetreat();
            else                          dislikeReroll();
            break;
        }
        default: break;
    }
}

// Train every live target toward its goal, retiring the ones that have got there (or
// outlived kDislikeTargetLifetimeMs). Checks the network's own (noise-free) output at
// the target's input.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::trainDislikeTargets(float lr) {
    if (dislikeTargets_.empty()) return;
    const uint32_t now = millis();
    training_pair_t batch;
    std::vector<float> y;
    for (size_t i = dislikeTargets_.size(); i-- > 0;) {
        const auto& t = dislikeTargets_[i];
        bool done = (now - t.t0) >= kDislikeTargetLifetimeMs;
        if (!done) {
            synthMapping.GetOutput(t.input, &y);
            const float toTarget = rmsDistActive(y, t.target);
            done = toTarget < kSeekEpsilon
                || (t.kind == DislikeTarget::Kind::REPEL && rmsDistActive(y, t.origin) >= kRepelMargin);
        }
        if (done) {
            dislikeTargets_.erase(dislikeTargets_.begin() + i);
            continue;
        }
        batch.first.push_back(t.input);
        batch.second.push_back(t.target);
    }
    if (!batch.first.empty())
        synthMapping.TrainBatch(batch, lr, 1, batch.first.size(), 0.f, false);
}


// ─── NN Inputs screen ───────────────────────────────────────────────────────────────

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::configureInputsView() {
    if (!nnInputsGraphView) return;
    static const char* joy[] = {"X", "Y", "Z", "W"};
    std::vector<String> labels;
    auto addJoy  = [&](size_t n) { for (size_t i = 0; i < n; i++) labels.push_back(joy[i]); };
    auto addMidi = [&](size_t n) { for (size_t i = 0; i < n; i++) labels.push_back("CC" + String(i + 1)); };
    auto addML   = [&]() { for (size_t i = 0; i < 6; i++) labels.push_back(i < mlLabels_.size() ? mlLabels_[i] : String("")); };
    bool map = false;
    switch (input_source_) {
        case INPUT_SOURCE::JOYSTICK_3D:       addJoy(3); map = true; break;
        case INPUT_SOURCE::JOYSTICK_4D:       addJoy(4); map = true; break;
        case INPUT_SOURCE::MACHINE_LISTENING: addML(); break;
        case INPUT_SOURCE::MIDI_1CC:          addMidi(1); break;
        case INPUT_SOURCE::MIDI_3CC:          addMidi(3); break;
        case INPUT_SOURCE::MIDI_8CC:          addMidi(8); break;
        case INPUT_SOURCE::COMBINED:          addJoy(4); addML(); break;
        default: break;
    }
    labels.resize(std::min(labels.size(), getActiveInputCount()));
    nnInputsGraphView->setSource(inputSourceName(input_source_), labels, map);
}

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::pushMemoryPointsToInputsView() {
    if (!nnInputsGraphView || !nnInputsGraphView->IsVisible() || !nnInputsGraphView->isMapMode())
        return;
    auto q = [](float v) {
        v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
        return static_cast<uint8_t>(v * 255.f + 0.5f);
    };
    std::vector<InputsView::MemPoint> pts;
    pts.reserve(replayMem.size());
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.input.size() < 2) continue;
        const bool has4 = item.input.size() >= 4;
        pts.push_back({q(item.input[0]), q(item.input[1]),
                       has4 ? q(item.input[2]) : uint8_t(0), has4 ? q(item.input[3]) : uint8_t(0),
                       item.reward > 0.f});
    }
    for (const auto& sp : synth_) {
        if (sp.input.size() < 4) continue;
        InputsView::MemPoint mp{q(sp.input[0]), q(sp.input[1]), q(sp.input[2]), q(sp.input[3]), true};
        mp.synth = true;
        pts.push_back(mp);
    }
    nnInputsGraphView->setMemoryPoints(pts);
}


// ─── Jolt ──────────────────────────────────────────────────────────────────────────

// Plant a fresh set of kSynthPoints synthetic points (replacing the last). Inputs: random
// in the playable range, clear of the liked inputs and of each other (a random acceptable
// candidate, not the most extreme; the furthest if none is clear). Targets: each focused
// output dim goes to the opposite half of its range from what the net plays there now,
// so every jolted sound is a big change. Unfocused dims keep the current output.
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::seedJolt() {
    synth_.clear();
    auto rnd = []() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };
    const size_t nAct = std::min(getActiveInputCount(), controlInput.size());
    const float clearance = kSynthClearance * sqrtf(static_cast<float>(std::max<size_t>(nAct, 1)));
    std::vector<float> cand(controlInput.size(), unusedInputDefault_);
    for (int p = 0; p < kSynthPoints; p++) {
        std::vector<float> best;
        float bestDist = -1.f;
        bool found = false;
        for (int c = 0; c < kSynthCandidates && !found; c++) {
            for (size_t i = 0; i < nAct; i++) cand[i] = kSynthInputLo + rnd() * (kSynthInputHi - kSynthInputLo);
            float dLike = 1e9f, dSynth = 1e9f;
            for (size_t i = 0; i < replayMem.size(); i++) {
                const auto& item = replayMem.getItem(i);
                if (item.reward > 0.f) dLike = std::min(dLike, euclideanDistance(item.input, cand));
            }
            for (const auto& sp : synth_) dSynth = std::min(dSynth, euclideanDistance(sp.input, cand));
            const float d = std::min(dLike, dSynth * 2.f);
            if (dLike >= clearance && dSynth >= clearance * 0.5f) found = true;  // take it
            if (found || d > bestDist) { bestDist = d; best = cand; }
        }
        SynthPoint sp;
        sp.input = best;
        evalOut_.resize(N_OUTPUTS);
        synthMapping.GetOutput(sp.input, &evalOut_);
        sp.target = evalOut_;
        const float mid = 0.5f * (kSynthTargetLo + kSynthTargetHi);
        for (size_t j = 0; j < sp.target.size(); j++) {
            if (!isActiveDim(j)) continue;
            sp.target[j] = (sp.target[j] < mid) ? mid + 0.05f + rnd() * (kSynthTargetHi - mid - 0.05f)
                                                : kSynthTargetLo + rnd() * (mid - 0.05f - kSynthTargetLo);
        }
        synth_.push_back(std::move(sp));
    }
    synthSeedMs_ = millis();
    synthSeedError_ = synthError();
    synthBurst_ = true;
    wakeTraining();
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("jolt");
}

// One training step towards the synthetic points: a burst at kSynthBurstLR after a
// jolt, until the synthetic error is down to kSynthBurstDone of where it began (or
// kSynthBurstMaxMs), then kSynthHoldLR as anchors. Returns true during the burst.
template<size_t N_OUTPUTS, size_t N_INPUTS>
bool InterfaceRL<N_OUTPUTS, N_INPUTS>::trainSynth(float lr) {
    if (synth_.empty()) return false;
    if (synthBurst_) {
        const uint32_t age = millis() - synthSeedMs_;
        // Check progress a few times a second (a full pass over 8 points).
        static uint32_t lastCheck = 0;
        if (millis() - lastCheck >= 200) {
            lastCheck = millis();
            const float e = synthError();
            if (e <= synthSeedError_ * kSynthBurstDone || age >= kSynthBurstMaxMs) {
                synthBurst_ = false;
            }
        }
    }
    training_pair_t ts;
    ts.first.reserve(synth_.size());
    ts.second.reserve(synth_.size());
    for (const auto& sp : synth_) {
        ts.first.push_back(sp.input);
        ts.second.push_back(sp.target);
    }
    synthMapping.TrainBatch(ts, lr * (synthBurst_ ? kSynthBurstLR : kSynthHoldLR), 1, synth_.size(), 0.f, false);
    return synthBurst_;
}

// Mean MSE between the net and the synthetic targets (no update).
template<size_t N_OUTPUTS, size_t N_INPUTS>
float InterfaceRL<N_OUTPUTS, N_INPUTS>::synthError() {
    float sum = 0.f;
    evalOut_.resize(N_OUTPUTS);
    for (const auto& sp : synth_) {
        synthMapping.GetOutput(sp.input, &evalOut_);
        float se = 0.f;
        for (size_t j = 0; j < sp.target.size(); j++) { const float d = sp.target[j] - evalOut_[j]; se += d * d; }
        sum += se / sp.target.size();
    }
    return synth_.empty() ? 0.f : sum / synth_.size();
}


// ─── Exploration noise (weight-space) ───────────────────────────────────────────────

template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::initExploreNoise() {
    exploreOU_.clear();
    exploreOU_.reserve(kWeightDirs);
    for (size_t i = 0; i < kWeightDirs; i++) {
        // OU(theta, mu, sigma, dt, x0): correlation time ~1/(theta*dt) calls (~60s at
        // 200Hz), so the mapping morphs in long smooth sweeps. Unit std, scaled where used.
        exploreOU_.emplace_back(0.02f, 0.0f, 0.0f, kNoiseDt, 0.0f);
        exploreOU_.back().setStationaryStd(1.f);
    }
    // Fixed random +-1 weight directions (a simple LCG, so they're the same every boot).
    uint32_t r = 0x12345678u;
    for (size_t k = 0; k < kWeightDirs; k++)
        for (size_t o = 0; o < N_OUTPUTS; o++)
            for (size_t h = 0; h < kHidden; h++) {
                r = r * 1664525u + 1013904223u;
                weightDirs_[k][o][h] = (r >> 31) ? 1 : -1;
            }
}

// Noise scale at input x: kNearFloor right on a liked input, rising to 1 at kNearRadius.
template<size_t N_OUTPUTS, size_t N_INPUTS>
float InterfaceRL<N_OUTPUTS, N_INPUTS>::noiseLocality(const std::vector<float>& x) const {
    const size_t nAct = std::max<size_t>(1, getActiveInputCount());
    const float radius = kNearRadius * sqrtf(static_cast<float>(nAct));
    float d = 1e9f;
    for (size_t i = 0; i < replayMem.size(); i++) {
        const auto& item = replayMem.getItem(i);
        if (item.reward > 0.f) d = std::min(d, euclideanDistance(item.input, x));
    }
    if (d >= radius) return 1.f;
    return kNearFloor + (1.f - kNearFloor) * (d / radius);
}

// The net's forward pass done by hand (same maths as StaticMLP: ReLU, ReLU, hard
// sigmoid) with the last layer's weights perturbed: dW = sum_k c_k D_k, so the output
// pre-activation shifts by dz_o = sum_k c_k (D_k[o] . h2). Normalised by |h2| and sqrt(K)
// to ~unit std per output, then x6 (the hard sigmoid's slope is 1/6) so each output moves
// by ~noiseAmp_ (scaled down near the likes).
template<size_t N_OUTPUTS, size_t N_INPUTS>
void InterfaceRL<N_OUTPUTS, N_INPUTS>::noisyForward(std::vector<float>& out) {
    const float amp = noiseAmp_ * noiseLocality(controlInput);
    const auto& L0 = synthMapping.template layer<0>();
    const auto& L1 = synthMapping.template layer<1>();
    const auto& L2 = synthMapping.template layer<2>();

    float h1[kHidden], h2[kHidden];
    for (size_t j = 0; j < kHidden; j++) {
        float z = L0.bias(j);
        for (size_t i = 0; i < N_INPUTS && i < controlInput.size(); i++) z += L0.weight(j, i) * controlInput[i];
        h1[j] = z > 0.f ? z : 0.f;
    }
    float hn = 0.f;
    for (size_t j = 0; j < kHidden; j++) {
        float z = L1.bias(j);
        for (size_t i = 0; i < kHidden; i++) z += L1.weight(j, i) * h1[i];
        h2[j] = z > 0.f ? z : 0.f;
        hn += h2[j] * h2[j];
    }
    hn = sqrtf(hn);
    const float wScale = hn > 1e-6f ? 6.f * amp / (hn * sqrtf(static_cast<float>(kWeightDirs))) : 0.f;
    float c[kWeightDirs];
    for (size_t k = 0; k < kWeightDirs; k++) c[k] = exploreOU_[k].sample();

    out.resize(N_OUTPUTS);
    for (size_t o = 0; o < N_OUTPUTS; o++) {
        float z = L2.bias(o);
        for (size_t i = 0; i < kHidden; i++) z += L2.weight(o, i) * h2[i];
        float dz = 0.f;
        for (size_t k = 0; k < kWeightDirs; k++) {
            float dot = 0.f;
            for (size_t j = 0; j < kHidden; j++) dot += weightDirs_[k][o][j] * h2[j];
            dz += c[k] * dot;
        }
        z += dz * wScale;
        out[o] = z <= -3.f ? 0.f : (z >= 3.f ? 1.f : (z + 3.f) / 6.f);
    }
}
