#include "MIDIInOut.hpp"
#include <Arduino.h>
#include "../PicoDefs.hpp"
#include <unordered_set>



struct CustomMIDISettings : public midi::DefaultSettings {
    static const bool UseRunningStatus = false;
    static const bool Use1ByteParsing = false;  // Add this line
    static const long BaudRate = 31250;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial2, MIDI, CustomMIDISettings);

MIDIInOut* MIDIInOut::instance_ = nullptr;

MIDIInOut::MIDIInOut() : n_outputs_(0),
                         cc_callback_(nullptr),
                         note_callback_(nullptr),
                         send_channel_(1),
                         note_channel_(1),
                         refresh_uart_(false),
                         use_advanced_mappings_(false) {
    instance_ = this;
}

void MIDIInOut::Setup(size_t n_outputs,
    bool midi_through, uint8_t midi_tx, uint8_t midi_rx) {
    n_outputs_ = n_outputs;
    cc_numbers_.resize(n_outputs);

    // Initialize Serial2 first
    Serial2.end(); // Reset Serial2 state
    delay(10);
    Serial2.setFIFOSize(32); // Set larger FIFO
    Serial2.setTX(midi_tx);
    Serial2.setRX(midi_rx);
    Serial2.begin(31250); // Start MIDI baud rate
    while(!Serial2) { delay(1); } // Wait for Serial2
    delay(100); // Allow port to stabilize

    // Initialize CC numbers to default values [0 .. (n_outputs-1)]
    std::vector<int>  midiShitList = {0, 6,7, 8, 10, 32, 38, 39,41, 43};

    std::unordered_set<int> excludeSet(midiShitList.begin(), midiShitList.end());
    std::vector<int> ccnums;
    ccnums.reserve(127 - midiShitList.size()); // pre-allocate

    for(int i = 0; i < 127; i++) {
        if(excludeSet.find(i) == excludeSet.end()) {
            ccnums.push_back(i);
        }
    }

    cc_numbers_.resize(ccnums.size());
    for(size_t i=0; i < ccnums.size();i++) {
        cc_numbers_[i] = static_cast<uint8_t>(ccnums[i]);
    }

    if (midi_through) {
        // Enable MIDI thru if requested
        MIDI.turnThruOn();
    } else {
        // Disable MIDI thru to prevent loop-back
        MIDI.turnThruOff();
    }

    // Setup static callbacks
    MIDI.setHandleControlChange(handleControlChange);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.begin(MIDI_CHANNEL_OMNI);
    delay(100); // Allow MIDI to initialize

    // // Test sending messages with memory barriers
    // MEMORY_BARRIER();
    // MIDI.sendNoteOn(60, 127, 1);
    // Serial2.flush();
    // delay(1);
    // MIDI.sendNoteOff(60, 0, 1);
    // Serial2.flush();
    // delay(1);
    // MIDI.sendControlChange(102, 127, 1);
    // Serial2.flush();
    // delay(1);
    // MIDI.sendControlChange(102, 0, 1);
    // Serial2.flush();
    // MEMORY_BARRIER();

     DEBUG_PRINTLN("MIDI setup complete");
    // Serial2.flush();  // Ensure the message is sent completely
}

void MIDIInOut::Poll()
{
    // RefreshUART_(); // Refresh UART if needed

    MIDI.read();
}


void MIDIInOut::warnSizeMismatch(const char* function_name, size_t expected, size_t actual) const {
    DEBUG_PRINTF("Warning: %s size mismatch (expected %d, got %d)\n", function_name, expected, actual);
}

void MIDIInOut::SetParamCCNumbers(const std::vector<uint8_t>& cc_numbers) {
    if (cc_numbers.size() != n_outputs_) {
        warnSizeMismatch("SetParamCCNumbers", n_outputs_, cc_numbers.size());
    }
    cc_numbers_ = cc_numbers;
}

void MIDIInOut::SendParamsAsMIDICC(const std::vector<float>& params) {
    RefreshUART_(); // Refresh UART if needed

    while(!Serial2) { delay(1); } // Wait for Serial2

    if (use_advanced_mappings_) {
        // Use advanced mappings with individual channels and custom scaling
        size_t send_count = std::min(params.size(), std::min(advanced_mappings_.size(), n_outputs_));
        for (size_t i = 0; i < send_count; i++) {
            const CCMapping& mapping = advanced_mappings_[i];
            uint8_t value = scaleValue(params[i], mapping);
            MIDI.sendControlChange(mapping.cc_number, value, mapping.channel);
        }
    } else {
        // Use simple mappings (backwards compatible)
        size_t send_count = std::min(params.size(), std::min(cc_numbers_.size(), n_outputs_));
        for (size_t i = 0; i < send_count; i++) {
            float clamped = std::max(0.0f, std::min(1.0f, params[i]));
            uint8_t value = static_cast<uint8_t>(clamped * 127.0f + 0.5f);
            MIDI.sendControlChange(cc_numbers_[i], value, send_channel_);
        }
    }
    MEMORY_BARRIER();
}

void MIDIInOut::SetAdvancedParamMappings(const std::vector<CCMapping>& mappings) {
    if (mappings.size() != n_outputs_) {
        warnSizeMismatch("SetAdvancedParamMappings", n_outputs_, mappings.size());
    }

    advanced_mappings_ = mappings;

    // Validate and fix mappings for efficiency
    for (size_t i = 0; i < advanced_mappings_.size(); i++) {
        // Ensure valid ranges
        if (advanced_mappings_[i].channel < 1 || advanced_mappings_[i].channel > 16) {
            advanced_mappings_[i].channel = 1;
        }
        if (advanced_mappings_[i].cc_number > 127) {
            advanced_mappings_[i].cc_number = 127;
        }
        if (advanced_mappings_[i].min_value > 127) {
            advanced_mappings_[i].min_value = 127;
        }
        if (advanced_mappings_[i].max_value > 127) {
            advanced_mappings_[i].max_value = 127;
        }
        // Recompute scale factor in case values were clamped
        uint8_t range = advanced_mappings_[i].max_value - advanced_mappings_[i].min_value;
        advanced_mappings_[i].scale_factor = range;
    }

    use_advanced_mappings_ = true;
}

void MIDIInOut::SetParamMapping(size_t index, uint8_t cc_number, uint8_t channel, uint8_t min_value, uint8_t max_value) {
    if (index >= n_outputs_) {
        DEBUG_PRINTF("Warning: Parameter index %d out of range (max %d)\n", index, n_outputs_ - 1);
        return;
    }

    // Initialize advanced_mappings_ if not already done
    if (advanced_mappings_.size() != n_outputs_) {
        advanced_mappings_.resize(n_outputs_);
    }

    // Validate and clamp values using bit operations for efficiency
    channel = (channel < 1) ? 1 : ((channel > 16) ? 16 : channel);
    cc_number = (cc_number > 127) ? 127 : cc_number;
    min_value = (min_value > 127) ? 127 : min_value;
    max_value = (max_value > 127) ? 127 : max_value;

    advanced_mappings_[index] = CCMapping(cc_number, channel, min_value, max_value);
    use_advanced_mappings_ = true;
}

void MIDIInOut::ClearAdvancedMappings() {
    use_advanced_mappings_ = false;
}

void MIDIInOut::SetMIDISendChannel(uint8_t channel) {
    // Ensure channel is in valid range (1-16)
    if (channel < 1 || channel > 16) {
        DEBUG_PRINTF("Warning: Invalid MIDI channel %d (must be 1-16)\n", channel);
        return;
    }
    send_channel_ = channel;
}

void MIDIInOut::SetMIDINoteChannel(uint8_t channel) {
    // Ensure channel is in valid range (1-16)
    if (channel < 1 || channel > 16) {
        DEBUG_PRINTF("Warning: Invalid MIDI note channel %d (must be 1-16)\n", channel);
        return;
    }
    note_channel_ = channel;
}

void MIDIInOut::SetCCCallback(midi_cc_callback_t callback) {
    cc_callback_ = callback;
}

void MIDIInOut::SetNoteCallback(midi_note_callback_t callback) {
    note_callback_ = callback;
}

// Static callback implementations
void MIDIInOut::handleControlChange(byte channel, byte number, byte value) {
    if (instance_ && instance_->cc_callback_) {
        instance_->cc_callback_(number, value);
    }
}

void MIDIInOut::handleNoteOn(byte channel, byte note, byte velocity) {
    if (instance_ && instance_->note_callback_) {
        instance_->note_callback_(true, note, velocity);
    }
}

void MIDIInOut::handleNoteOff(byte channel, byte note, byte velocity) {
    if (instance_ && instance_->note_callback_) {
        instance_->note_callback_(false, note, velocity);
    }
}

void MIDIInOut::RefreshUART_(void) {
    if (!READ_VOLATILE(refresh_uart_)) {
        Serial2.end(); // Reset Serial2 state
        //delay(10);
        Serial2.setFIFOSize(32); // Set larger FIFO
        Serial2.setTX(Pins::MIDI_TX);
        Serial2.setRX(Pins::MIDI_RX);
        Serial2.begin(31250); // Start MIDI baud rate
        while(!Serial2) {} // Wait for Serial2
        WRITE_VOLATILE(refresh_uart_, true);

        DEBUG_PRINTLN("MIDI UART refreshed.");
    }
}

bool MIDIInOut::sendNoteOn(uint8_t note_number, uint8_t velocity) {
    if (note_number > 127 || velocity > 127) {
        return false;
    }

    RefreshUART_(); // Refresh UART if needed

    while(!Serial2) { delay(1); } // Wait for Serial2
    MIDI.sendNoteOn(note_number, velocity, note_channel_);
    MEMORY_BARRIER();
    return true;
}

bool MIDIInOut::sendNoteOff(uint8_t note_number, uint8_t velocity) {
    if (note_number > 127 || velocity > 127) {
        return false;
    }

    RefreshUART_(); // Refresh UART if needed

    while(!Serial2) { delay(1); } // Wait for Serial2
    MIDI.sendNoteOff(note_number, velocity, note_channel_);
    MEMORY_BARRIER();
    return true;
}
