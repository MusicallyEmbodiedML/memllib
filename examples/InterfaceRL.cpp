#include "InterfaceRL.hpp"
#include "../utils/sharedMem.hpp" // Required for READ_VOLATILE, sharedMem constants and PERIODIC_DEBUG
#include <Arduino.h>     // Required for Serial, millis, delay
#include "../hardware/memlnaut/MEMLNaut.hpp" // Required for MEMLNaut::Instance()
// display.hpp is included via InterfaceRL.hpp




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
    if (nnOutputsGraphView) nnOutputsGraphView->flashCommand("LIKE");
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
    if (nnOutputsGraphView) nnOutputsGraphView->flashCommand("DISLIKE");
    DEBUG_PRINTLN(msg);
    if (msgView) msgView->post(msg);
}

void InterfaceRL::_perform_randomiseRL_action() {

    this->randomiseTheNetwork();
    this->generateAction(true);
    if (nnOutputsGraphView) nnOutputsGraphView->flashCommand("RANDOMISE");
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


void InterfaceRL::bind_RL_interface(INPUT_MODES input_mode, bool joystick4D) {

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


    if (input_mode != INPUT_MODES::MACHINE_LISTENING &&
        input_mode != INPUT_MODES::SERIAL_INPUT) {
        MEMLNaut::Instance()->setJoySWCallback([this](bool state) {
            //button down
            if (state) {
                //store output
                savedAction = action;
                actionBeingDragged=true;
                msgView->post("Where do you want it?");
            }else{
                //button up
                if (actionBeingDragged) {
                    this->storeExperience(1.f, controlInput, savedAction);
                    actionBeingDragged=false;
                    msgView->post("Here!");
                }
            }
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

        if (joystick4D) {
            MEMLNaut::Instance()->setADC3Callback([this](float value) {
                this->setState(3, value);
            });
        }

        if (input_mode == INPUT_MODES::JOYSTICK_AND_MACHINE_LISTENING) {
            analysisParamsOffset = 3 + (joystick4D ? 1 : 0);
        } else {
            analysisParamsOffset = 0;
        }
    }


    MEMLNaut::Instance()->setTogB1Callback([this](bool state) { // scr_ref no longer captured directly
        if (state) {
            this->_forget_replay_mem_interf();
        }
    });

    MEMLNaut::Instance()->setTogA1Callback([this](bool state) { // scr_ref no longer captured directly
        //tiggle up
        if (state) {
            //store output
            savedAction = action;
            actionBeingDragged=true;
            msgView->post("Where do you want it?");
        }else{
            //button up
            if (actionBeingDragged) {
                this->storeExperience(1.f, controlInput, savedAction);
                actionBeingDragged=false;
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
    if (nnOutputsGraphView) nnOutputsGraphView->flashCommand("CLEAR");
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

    mlpActive = spin_lock_init(spin_lock_claim_unused(true));

    const std::vector<ACTIVATION_FUNCTIONS> activfuncs = {
        RELU, RELU, HARDSIGMOID
    };

    std::vector<size_t> layers_nodes = {
        n_inputs,
        16, 16,
        n_outputs
    };

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

    // GUI — Neural Net section
    Serial.println("DBG: creating nnSection");
    nnSection = std::make_shared<SectionView>("Neural Net");

    Serial.println("DBG: creating nnOutputsGraphView");
    nnOutputsGraphView = std::make_shared<BarGraphView>("NN Outputs", n_outputs, 4, TFT_GREEN, 0.f, 1.f);
    nnSection->addChild(nnOutputsGraphView);
    Serial.println("DBG: creating rlStatsView");
    rlStatsView = std::make_shared<RLStatsView>("RL Stats");
    nnSection->addChild(rlStatsView);
    Serial.println("DBG: creating nnInputsGraphView");
    nnInputsGraphView = std::make_shared<BarGraphView>("NN Inputs", n_inputs, 10, TFT_YELLOW, 0.f, 1.f);
    nnSection->addChild(nnInputsGraphView);

    Serial.println("DBG: creating memoryStoreModeView");
    memoryStoreModeView = std::make_shared<SingleSelectView>("Mem Mode");
    memoryStoreModeView->setOptions(memOptions);
    memoryStoreModeView->setNewVoiceCallback([this](size_t idx) {
        memoryStoreMode = static_cast<MEMORY_STORE_MODES>(idx);
    });
    nnSection->addChild(memoryStoreModeView);

    Serial.println("DBG: creating msgView");
    msgView = std::make_shared<MessageView>("Messages");
    nnSection->addChild(msgView);

    Serial.println("DBG: AddView(nnSection)");
    MEMLNaut::Instance()->disp->AddView(nnSection);
    Serial.println("DBG: nnSection added ok");

    // File section
    Serial.println("DBG: creating fileSection");
    fileSection = std::make_shared<SectionView>("File");

    Serial.println("DBG: creating fileSaveView");
    fileSaveView = std::make_shared<BlockSelectView>("Save Model", TFT_BLUE);
    fileSaveView->SetOnSelectCallback([this](size_t id) {
        pendingSaveSlot = static_cast<int>(id) - 1;
        nameInputView->reset(slotNames[pendingSaveSlot]);
        MEMLNaut::Instance()->disp->NavigateToView(nameInputView);
    });
    fileSection->addChild(fileSaveView);

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
    fileSection->addChild(fileLoadView);

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
            MEMLNaut::Instance()->disp->NavigateToView(fileSaveView);
        },
        [this]() {
            MEMLNaut::Instance()->disp->NavigateToView(fileSaveView);
        }
    );
    fileSection->addChild(nameInputView);
    Serial.println("DBG: AddView(fileSection)");
    MEMLNaut::Instance()->disp->AddView(fileSection);
    Serial.println("DBG: fileSection added ok");
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


        for(auto &i: sample) {
		//for(size_t i = 0; i < sample.size(); i++) {

            // Validate input and action sizes
            // if (sample[i].input.size() != n_inputs_ || sample[i].action.size() != n_outputs_) {
            //     Serial.printf("ERROR: Invalid sample %d - input_size=%d (expected %d), action_size=%d (expected %d)\n",
            //         i, sample[i].input.size(), n_inputs_, sample[i].action.size(), n_outputs_);
            //     continue;
            // }

            if (replayMem.getItem(i).reward > 0) {
                auto input = replayMem.getItem(i).input;
                auto action = replayMem.getItem(i).action;
                // Serial.printf("[DEBUG] Adding pos exp %d: reward=%f, input_size=%d, action_size=%d\n",
                //              i, replayMem.getItem(i).reward, input.size(), action.size());

                // Validate data before adding
                // if (input.empty() || action.empty()) {
                //     Serial.printf("[DEBUG] *** SKIPPING exp %d: empty input or action! ***\n", i);
                //     continue;
                // }

                tsPositive.first.push_back(input);
                tsPositive.second.push_back(action);
                batchSizePos++;
                avgRewardPos+=replayMem.getItem(i).reward;
            }else{
                auto input = replayMem.getItem(i).input;
                auto action = replayMem.getItem(i).action;

                // Validate data before adding
                // if (input.empty() || action.empty()) {
                //     Serial.printf("[DEBUG] *** SKIPPING neg exp %d: empty input or action! ***\n", i);
                //     continue;
                // }

                tsNegative.first.push_back(input);
                tsNegative.second.push_back(action);
                batchSizeNeg++;
                float reward = replayMem.getItem(i).reward;
                avgRewardNeg+=reward;
                //decay towards 0
                const float rewardCom = 1.f+ reward;
                reward = reward + (0.005 * rewardCom);
				replayMem.getItem(i).reward = reward; 
                if (reward > -0.01) {
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
            float effectiveLR_neg = learningRateScaled * 0.1f * avgRewardNeg;
            // Serial.printf("[DEBUG] Neg batch: size=%d, avgReward=%f, LR=%f, effectiveLR=%f\n",
            //              batchSizeNeg, avgRewardNeg, learningRateScaled, effectiveLR_neg);
            lossNegative = synthMapping->TrainBatch(tsNegative, learningRateScaled * 0.3 * avgRewardNeg, 1, batchSize, 0.f, false);
            // Serial.printf("[DEBUG] Loss after negative TrainBatch: %f (inf=%d, nan=%d)\n",
            //              lossNegative, std::isinf(lossNegative), std::isnan(lossNegative));
        }

        if (replayMem.removeItems(itemsToRemove)) {
            itemsToRemove.clear();
        };

        // if (std::isinf(lossPositive) || std::isnan(lossPositive)) {
        //     Serial.printf("WARNING: Invalid loss detected!\n");
        //     lossPositive = 0.f;
        // }        
        rlStatsView->setLoss(lossPositive);
        if (nnOutputsGraphView) nnOutputsGraphView->setStatus(
            lossPositive, replayMem.size(), learningRateScaled,
            ou_noises.empty() ? 0.f : ou_noises[0]->getSigma());
        // Serial.printf("l %f\n", lossPositive);

    }
}

void InterfaceRL::readAnalysisParameters(std::vector<float> params) {
    //copy analysis parameters into control input vector
    for(size_t i=0; i < params.size(); i++) {
        controlInput[analysisParamsOffset+i] = params[i];
    }
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

float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

void InterfaceRL::removeItemsAtDistance(std::vector<float> &experienceState, const float distThreshold) {
    std::vector<size_t> indicesToRemove;
    for(size_t i=0; i < replayMem.size(); i++) {
        const trainStatelessRLItem& item = replayMem.getItem(i);   
        float dist = euclideanDistance(item.input, experienceState);
        if (dist < distThreshold) { 
            indicesToRemove.push_back(i); 
            if (msgView) msgView->post("Removing similar memory item");
        }
    }
    replayMem.removeItems(indicesToRemove);             
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
    switch(memoryStoreMode) {
        case MEMORY_STORE_MODES::ADD:
            // Already added above
            break;
        case MEMORY_STORE_MODES::REPLACE_5_PERCENT:
        {
            removeItemsAtDistance(experienceState, 0.05f);
            break;
        }
        case MEMORY_STORE_MODES::REPLACE_15_PERCENT:
        {
            removeItemsAtDistance(experienceState, 0.15f);
            break;
        }
        case MEMORY_STORE_MODES::REWARD_DECAY_10_PERCENT:
            decayItemsAtDistance(experienceState, 0.10f);
            break;
        case MEMORY_STORE_MODES::REWARD_DECAY_20_PERCENT:
            decayItemsAtDistance(experienceState, 0.20f);
            break;
    }
    replayMem.add(trainItem, millis());
}
