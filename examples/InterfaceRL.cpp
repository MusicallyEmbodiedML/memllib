#include "InterfaceRL.hpp"
#include <LittleFS.h>
#include "../utils/sharedMem.hpp" // Required for READ_VOLATILE, sharedMem constants and PERIODIC_DEBUG
#include <Arduino.h>     // Required for Serial, millis, delay
#include "../hardware/memlnaut/MEMLNaut.hpp" // Required for MEMLNaut::Instance()
// display.hpp is included via InterfaceRL.hpp

float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}



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
    dislikeMultiplier_ = 1;
    if (nnOutputsGraphView) {
        nnOutputsGraphView->setLastAction("yes");
        nnOutputsGraphView->setDislikeMultiplier(dislikeMultiplier_);
    }
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
    // Accumulate into an existing nearby dislike rather than storing N identical copies
    bool accumulated = false;
    for (size_t i = 0; i < replayMem.size(); i++) {
        auto& item = replayMem.getItem(i);
        if (item.reward < 0.f && euclideanDistance(item.input, controlInput) < 0.05f) {
            item.reward = std::max(item.reward - 1.0f, -static_cast<float>(kMaxDislikeMultiplier));
            accumulated = true;
            break;
        }
    }
    if (!accumulated)
        this->storeExperience(-1.f, controlInput, action);
    dislikeMultiplier_ = std::min(dislikeMultiplier_ * 2, kMaxDislikeMultiplier);
    if (nnOutputsGraphView) {
        nnOutputsGraphView->setLastAction("no");
        nnOutputsGraphView->setDislikeMultiplier(dislikeMultiplier_);
    }
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_randomiseRL_action() {

    this->randomiseTheNetwork();
    this->generateAction(true);
    if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("scramble");
    DEBUG_PRINTLN("Randomising networks");
    if (msgView) msgView->post("Scrambling the network");
}

// Public trigger methods — called from ISR context, so only set a flag.
// The actual action runs in the main-loop loopCallback before optimise().
void InterfaceRL::trigger_like() {
    pendingLike_ = true;
}

void InterfaceRL::trigger_dislike() {
    pendingDislike_ = true;
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


void InterfaceRL::bind_RL_interface(INPUT_MODES input_mode, bool joystick4D) {

    loadInputSource();
    if (nnInputsGraphView) nnInputsGraphView->setNumDisplayBars(getActiveInputCount());

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
    MEMLNaut::Instance()->setMomB2Callback([this]() {
        if (MEMLNaut::Instance()->getMOMB2State()) {
            joltNetworks();
        }
    });

    // Always register joystick callbacks — they write to raw_joystick_
    // (ignored by assembleInputs() when a non-joystick source is active)
    MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
        if (state) {
            savedAction = action;
            actionBeingDragged = true;
            if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drag");
            msgView->post("Where do you want it?");
        } else {
            if (actionBeingDragged) {
                actionBeingDragged = false;
                pendingDragStore_ = true;
                if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drop");
                msgView->post("Here!");
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
            msgView->post("Where do you want it?");
        } else {
            if (actionBeingDragged) {
                actionBeingDragged = false;
                pendingDragStore_ = true;  // deferred: storeExperience in loopCallback
                if (nnOutputsGraphView) nnOutputsGraphView->setLastAction("drop");
                msgView->post("Here!");
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
            if (nnOutputsGraphView) {
                size_t pos = 0;
                for (size_t i = 0; i < replayMem.size(); i++)
                    if (replayMem.getItem(i).reward > 0.f) pos++;
                nnOutputsGraphView->setMemoryCounts(pos, replayMem.size() - pos);
            }
        }
        uint32_t save = spin_lock_blocking(mlpActive);
        this->optimiseSometimes();
        this->generateAction();
        spin_unlock(mlpActive, save);
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
    dislikeMultiplier_ = 1;
    if (nnOutputsGraphView) {
        nnOutputsGraphView->setLastAction("forget");
        nnOutputsGraphView->setDislikeMultiplier(dislikeMultiplier_);
    }
    static APP_SRAM std::vector<String> forgetmsgs = {
        "Erasing my memory", "Forgetting everything", "Memory wiped","Thank you Susan?",
        "Starting afresh", "Why care about the past?","Living in the moment"
    };
    String msg = forgetmsgs[rand() % forgetmsgs.size()];

    if (msgView) msgView->post(msg);
}


void InterfaceRL::bindMIDI(std::shared_ptr<MIDIInOut> midi_interf, bool enableFootcontroller)
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

    if (ccSelectView && !ccSelectView->getSelectedCCs().empty()) {
        midi_->SetParamCCNumbers(ccSelectView->getSelectedCCs());
    }
}

void InterfaceRL::setup(size_t n_inputs, size_t n_outputs)
{

    InterfaceBase::setup(n_inputs, n_outputs);

    mlpActive = spin_lock_init(spin_lock_claim_unused(true));

    const std::vector<ACTIVATION_FUNCTIONS> activfuncs = {
        RELU, RELU, HARDSIGMOID
    };

    layers_nodes = { n_inputs, 16, 16, n_outputs };

    controlInput.resize(layers_nodes[0]);
    action.resize(n_outputs, 0.5f);  // Initialize action vector with default values
    mappingOutput.resize(n_outputs);

    //init networks
    synthMapping = std::make_shared<MLP<float> > (
        layers_nodes,
        activfuncs,
        loss::LOSS_MSE,
        0,
        0
    );

    // synthMapping->InitXavier();
    synthMapping->RandomiseWeightsAndBiasesLin(-1.2f,0.9f, 0, 0.5);

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
    if (!nnOutputsGraphView) {
        nnOutputsGraphView = std::make_shared<RLView>("RL", n_outputs, 4, TFT_GREEN, 0.f, 1.f);
    }
    MEMLNaut::Instance()->disp->AddView(nnOutputsGraphView);
    nnInputsGraphView = std::make_shared<BarGraphView>("NN Inputs", n_inputs, 10, TFT_YELLOW, 0.f, 1.f);
    MEMLNaut::Instance()->disp->AddView(nnInputsGraphView);

    // memoryStoreModeView = std::make_shared<SingleSelectView>("Mem Mode");
    // MEMLNaut::Instance()->disp->AddView(memoryStoreModeView);
    // memoryStoreModeView->setOptions(memOptions);
    // memoryStoreModeView->setNewVoiceCallback([this](size_t idx) {
    //     memoryStoreMode = static_cast<MEMORY_STORE_MODES>(idx);
    // });

    msgView = std::make_shared<MessageView>("Messages");
    MEMLNaut::Instance()->disp->AddView(msgView);

    fileSaveView = std::make_shared<BlockSelectView>("Save Model", TFT_BLUE);
    fileSaveView->SetOnSelectCallback([this](size_t id) {
        pendingSaveSlot = static_cast<int>(id) - 1;
        nameInputView->reset(slotNames[pendingSaveSlot]);
        MEMLNaut::Instance()->disp->ShowDialog(nameInputView);
    });
    MEMLNaut::Instance()->disp->AddView(fileSaveView);

    fileLoadView = std::make_shared<BlockSelectView>("Load Model", TFT_PURPLE);
    fileLoadView->SetOnSelectCallback([this](size_t id) {
        int slotIdx = static_cast<int>(id) - 1;
        String filename = (slotNames[slotIdx].length() > 0) ? slotNames[slotIdx] : String(id);
        fileLoadView->SetMessage("Loading " + filename);
        uint32_t save = spin_lock_blocking(mlpActive);
        if (MEMLNaut::Instance()->startSD()) {
            if (this->_load_RL_from_SD(filename)) {
                fileLoadView->SetMessage("Loaded " + filename);
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
                slotNames[pendingSaveSlot] = name;
                fileSaveView->updateButtonName(static_cast<size_t>(pendingSaveSlot), displayName);
                fileLoadView->updateButtonName(static_cast<size_t>(pendingSaveSlot), displayName);
                fileSaveView->SetMessage("Saving as " + displayName);
                uint32_t save = spin_lock_blocking(mlpActive);
                if (MEMLNaut::Instance()->startSD()) {
                    _saveSlotNames();
                    if (this->_save_RL_to_SD(displayName)) {
                        fileSaveView->SetMessage("Saved as " + displayName);
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


void InterfaceRL::setModeInfo(const String& modeRoot, const String& modeTag) {
    _modeRoot = modeRoot;
    _modeTag = modeTag;
    if (MEMLNaut::Instance()->startSD()) {
        _loadSlotNames();
        MEMLNaut::Instance()->stopSD();
    }
}

bool InterfaceRL::_save_RL_to_SD(String id) {
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

    bool success = synthMapping->SaveMLPNetworkToFile(file);
    file.close();
    return success;
}

bool InterfaceRL::_load_RL_from_SD(String id) {
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

    bool success = synthMapping->LoadMLPNetworkFromFile(file);
    file.close();

    if (success && synthMapping->get_num_inputs() != (int)controlInput.size()) {
        // Saved model has wrong input count — rebuild with current architecture
        const std::vector<ACTIVATION_FUNCTIONS> activfuncs = { RELU, RELU, HARDSIGMOID };
        synthMapping = std::make_shared<MLP<float>>(layers_nodes, activfuncs, loss::LOSS_MSE, 0, 0);
        synthMapping->RandomiseWeightsAndBiasesLin(-1.2f, 0.9f, 0, 0.5f);
        if (msgView) msgView->post("Model incompatible: wrong input size");
        return false;
    }
    return success;
}

void InterfaceRL::_saveSlotNames() {
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

void InterfaceRL::_loadSlotNames() {
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


void InterfaceRL::optimise() {
    // k-NN centroid: average the kCentroidK positive memories nearest to the current input.
    // Using all positives regardless of input context produces a phantom centroid when
    // likes span very different states — the k-NN version stays contextually relevant.
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
    const size_t totalPosCount = candidates.size();  // used for dynamic LR ratio
    if (posMemCount > 0)
        for (auto& v : meanPositiveAction) v /= static_cast<float>(posMemCount);

    std::vector<size_t> sample = replayMem.sampleIndices(batchSize);
    if (sample.size() >1) {
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


        // Positive batch: random sample (diversity for generalisation)
        for (auto &i : sample) {
            if (replayMem.getItem(i).reward > 0) {
                tsPositive.first.push_back(replayMem.getItem(i).input);
                tsPositive.second.push_back(replayMem.getItem(i).action);
                batchSizePos++;
                avgRewardPos += replayMem.getItem(i).reward;
            }
        }

        // Negative batch: scan ALL negatives so every dislike is guaranteed to push.
        // Decay is applied here too so all items age uniformly regardless of sampling.
        for (size_t i = 0; i < replayMem.size(); i++) {
            float reward = replayMem.getItem(i).reward;
            if (reward <= 0.f) {
                tsNegative.first.push_back(replayMem.getItem(i).input);
                tsNegative.second.push_back(replayMem.getItem(i).action);
                batchSizeNeg++;
                avgRewardNeg += reward;
                reward += 0.0025f * std::max(fabsf(reward), 1.0f);  // proportional decay: high-magnitude items expire in the same time as single dislikes
                replayMem.getItem(i).reward = reward;
                if (reward > -0.01f) {
                    itemsToRemove.push_back(i);
                }
            }
        }
        float lossPositive{0.f};
        float lossNegative{0.f};
        if (batchSizePos > 0){
            avgRewardPos /= static_cast<float>(batchSizePos);
            float effectiveLR_pos = learningRateScaled * avgRewardPos;
            // Serial.printf("[DEBUG] Pos batch: counter=%d, actual_data_size=%d (inputs), %d (outputs), avgReward=%f, LR=%f, effectiveLR=%f\n",
                        //  batchSizePos, tsPositive.first.size(), tsPositive.second.size(),
                        //  avgRewardPos, learningRateScaled, effectiveLR_pos);

            // Check first sample for inf/nan
            // if (!tsPositive.first.empty() && !tsPositive.first[0].empty()) {
            //     bool hasInf = false;
            //     for (const auto& val : tsPositive.first[0]) {
            //         if (std::isinf(val) || std::isnan(val)) {
            //             Serial.printf("[DEBUG] *** TRAINING DATA HAS INF/NAN in input! ***\n");
            //             hasInf = true;
            //             break;
            //         }
            //     }
            //     if (!hasInf && !tsPositive.second.empty() && !tsPositive.second[0].empty()) {
            //         for (const auto& val : tsPositive.second[0]) {
            //             if (std::isinf(val) || std::isnan(val)) {
            //                 Serial.printf("[DEBUG] *** TRAINING DATA HAS INF/NAN in target! ***\n");
            //                 break;
            //             }
            //         }
            //     }
            // }

            lossPositive = synthMapping->TrainBatch(tsPositive, learningRateScaled * avgRewardPos, 1, batchSize, 0.f, false);
            // Serial.printf("[DEBUG] Loss after positive TrainBatch: %f (inf=%d, nan=%d)\n",
            //              lossPositive, std::isinf(lossPositive), std::isnan(lossPositive));
        }
        if (batchSizeNeg > 0){
            avgRewardNeg /= static_cast<float>(batchSizeNeg);

            if (posMemCount > 0) {
                // Push disliked actions away from positive replay memory centroid
                MLP<float>::training_pair_t tsGeometric;
                tsGeometric.first = tsNegative.first;
                tsGeometric.second.reserve(tsNegative.second.size());

                float pushStep = std::clamp(fabsf(avgRewardNeg), 0.25f, 1.0f) * kGeometricPushScale;

                for (const auto& neg_action : tsNegative.second) {
                    // Fix 3: guard against size mismatch with old saved actions
                    const size_t dimCount = std::min(neg_action.size(), meanPositiveAction.size());
                    float len = 0.f;
                    std::vector<float> dir(dimCount);
                    for (size_t j = 0; j < dimCount; j++) {
                        dir[j] = neg_action[j] - meanPositiveAction[j];
                        len += dir[j] * dir[j];
                    }
                    len = sqrtf(len);
                    const bool useRandom = (len <= 1e-4f);
                    // Taper push for items already far from the positive centroid
                    const float effectivePushStep = pushStep / (1.0f + len);
                    std::vector<float> target(neg_action);  // copy keeps out-of-range dims intact
                    for (size_t j = 0; j < dimCount; j++) {
                        bool active = activeDims_.empty() || (j < activeDims_.size() && activeDims_[j]);
                        if (!active) continue;
                        float d = useRandom
                            ? (static_cast<float>(rand() & 0xFF) / 127.5f - 1.f)
                            : (dir[j] / len);
                        target[j] = std::clamp(neg_action[j] + d * effectivePushStep, 0.f, 1.f);
                    }
                    tsGeometric.second.push_back(std::move(target));
                }
                // Dynamic LR ratio: push harder when dislikes are rare, gentler when they flood the buffer
                const float negFraction = static_cast<float>(batchSizeNeg)
                    / static_cast<float>(std::max(batchSizeNeg + totalPosCount, size_t{1}));
                const float negLRRatio = 0.5f - 0.4f * negFraction;
                lossNegative = synthMapping->TrainBatch(tsGeometric, learningRateScaled * negLRRatio, 1, batchSizeNeg, 0.f, false);
            } else {
                // Fallback: negative LR when replay memory has no positive items yet
                lossNegative = synthMapping->TrainBatch(tsNegative, learningRateScaled * 0.1f * avgRewardNeg, 1, batchSizeNeg, 0.f, false);
            }
        }

        // Fix 4: always clear — stale indices corrupt subsequent optimise() calls
        // Each expired no halves the multiplier (mirrors how it doubled on each dislike press)
        size_t expiredCount = itemsToRemove.size();
        replayMem.removeItems(itemsToRemove);
        itemsToRemove.clear();
        if (expiredCount > 0) {
            for (size_t i = 0; i < expiredCount; i++)
                dislikeMultiplier_ = std::max(dislikeMultiplier_ / 2, size_t{1});
            // If all nos are gone, fully reset regardless of halving remainder
            if (replayMem.size() == totalPosCount) dislikeMultiplier_ = 1;
            if (nnOutputsGraphView) nnOutputsGraphView->setDislikeMultiplier(dislikeMultiplier_);
        }

        // if (std::isinf(lossPositive) || std::isnan(lossPositive)) {
        //     Serial.printf("WARNING: Invalid loss detected!\n");
        //     lossPositive = 0.f;
        // }        
        if (nnOutputsGraphView) {
            nnOutputsGraphView->setLoss(lossPositive);
            nnOutputsGraphView->setMemoryCounts(totalPosCount, replayMem.size() - totalPosCount);
        }
        // Serial.printf("l %f\n", lossPositive);

    }
}

void InterfaceRL::readAnalysisParameters(std::vector<float> params) {
    for (size_t i = 0; i < params.size() && i < 6; i++) {
        raw_ml_[i] = params[i];
    }
    generateAction(true);
}

void InterfaceRL::assembleInputs() {
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

void InterfaceRL::copyAndZero(const float* src, size_t n) {
    size_t i = 0;
    for (; i < n && i < kMaxNNInputs; ++i) controlInput[i] = src[i];
    for (; i < kMaxNNInputs; ++i)           controlInput[i] = 0.f;
}

void InterfaceRL::saveInputSource() {
    FILE* f = fopen(kInputSourceFile, "wb");
    if (f) { fwrite(&input_source_, sizeof(input_source_), 1, f); fclose(f); }
}

void InterfaceRL::loadInputSource() {
    FILE* f = fopen(kInputSourceFile, "rb");
    if (f) { fread(&input_source_, sizeof(input_source_), 1, f); fclose(f); }
}

void InterfaceRL::addInputSourceView(bool includeCCSelect) {
    static const String srcNames[] = {
        "3D Joystick", "4D Joystick", "Machine Listen",
        "MIDI Mod Whl", "MIDI 3 CC", "MIDI 8 CC", "Combined"
    };
    std::vector<INPUT_SOURCE> available = {
        INPUT_SOURCE::JOYSTICK_3D, INPUT_SOURCE::JOYSTICK_4D,
        INPUT_SOURCE::MIDI_1CC, INPUT_SOURCE::MIDI_3CC, INPUT_SOURCE::MIDI_8CC
    };
    if (hasMachineListening_) {
        available.push_back(INPUT_SOURCE::MACHINE_LISTENING);
        available.push_back(INPUT_SOURCE::COMBINED);
    }

    std::vector<String> opts;
    for (auto src : available) opts.push_back(srcNames[static_cast<size_t>(src)]);

    size_t initialSel = 0;
    auto it = std::find(available.begin(), available.end(), input_source_);
    if (it != available.end()) initialSel = std::distance(available.begin(), it);

    auto view = std::make_shared<RotarySelectView>("Input Source");
    view->setOptions(std::span<String>(opts.data(), opts.size()));
    view->setSelection(initialSel);
    view->setNewSelectionCallback([this, available](size_t idx) {
        if (idx < available.size()) setInputSource(available[idx]);
    });
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

void InterfaceRL::generateAction(bool donthesitate) {
    if (newInput || donthesitate) {
        newInput = false;

        assembleInputs();

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
        if (paramTransformHook) paramTransformHook(mappingOutput);
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


bool InterfaceRL::removeItemsAtDistance(std::vector<float> &experienceState, const float distThreshold, const float reward) {
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
            } else if ((item.reward > 0.f && reward > 0.f) || (item.reward < 0.f && reward < 0.f)) {
                indicesToRemove.push_back(i);
                if (msgView) msgView->post("Removing similar memory item");
            }
        }
    }
    replayMem.removeItems(indicesToRemove);
    return accumulated;
}

void InterfaceRL::decayItemsAtDistance(std::vector<float> &experienceState, const float distThreshold) {
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

void InterfaceRL::storeExperience(float reward, std::vector<float> &experienceState, std::vector<float> &experienceAction ) {
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

void InterfaceRL::saveCCNumbers() {
    if (!ccSelectView) return;
    String path = "/" + _modeRoot + "_cc_numbers.bin";
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        const auto& ccs = ccSelectView->getSelectedCCs();
        fwrite(ccs.data(), 1, ccs.size(), f);
        fclose(f);
    }
}

void InterfaceRL::loadCCNumbers() {
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
