#include "MIDIInOut.hpp"
#include <Arduino.h>
#include "../PicoDefs.hpp"


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
                         refresh_uart_(false) {
    instance_ = this;
}

void MIDIInOut::Setup(size_t n_outputs, uint8_t midi_tx, uint8_t midi_rx) {
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
    static const size_t cc_start = 14;
    for (size_t i = 0; i < n_outputs; i++) {
        cc_numbers_[i] = static_cast<uint8_t>(i + cc_start);
    }

    MIDI.turnThruOff();  // Add this line to prevent MIDI loop-back

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

    // Serial.println("MIDI setup complete");
    // Serial2.flush();  // Ensure the message is sent completely
}

void MIDIInOut::Poll()
{
    RefreshUART_(); // Refresh UART if needed

    MIDI.read();
}


void MIDIInOut::SetParamCCNumbers(const std::vector<uint8_t>& cc_numbers) {
    if (cc_numbers.size() != n_outputs_) {
        Serial.printf("Warning: CC numbers vector size mismatch (expected %d, got %d)\n",
                     n_outputs_, cc_numbers.size());
        return;
    }
    cc_numbers_ = cc_numbers;
}

void MIDIInOut::SendParamsAsMIDICC(const std::vector<float>& params) {
    if (params.size() != n_outputs_) {
        Serial.printf("Warning: Params vector size mismatch (expected %d, got %d)\n",
                     n_outputs_, params.size());
        return;
    }

    RefreshUART_(); // Refresh UART if needed

    while(!Serial2) { delay(1); } // Wait for Serial2
    for (size_t i = 0; i < n_outputs_; i++) {
        float clamped = std::max(0.0f, std::min(1.0f, params[i]));
        uint8_t value = static_cast<uint8_t>(clamped * 127.0f + 0.5f);
        MIDI.sendControlChange(cc_numbers_[i], value, send_channel_);
        //Serial2.write(0xB0 | (1 + cc_numbers_[i])); // Control Change message
        //Serial2.write(14); // CC number
        //Serial2.write(value); // CC value
        //Serial2.flush();
        //delayMicroseconds(1000); // Increase delay to 1ms for stability
    }
    MEMORY_BARRIER();
}

void MIDIInOut::SetMIDISendChannel(uint8_t channel) {
    // Ensure channel is in valid range (1-16)
    if (channel < 1 || channel > 16) {
        Serial.printf("Warning: Invalid MIDI channel %d (must be 1-16)\n", channel);
        return;
    }
    send_channel_ = channel;
}

void MIDIInOut::SetMIDINoteChannel(uint8_t channel) {
    // Ensure channel is in valid range (1-16)
    if (channel < 1 || channel > 16) {
        Serial.printf("Warning: Invalid MIDI note channel %d (must be 1-16)\n", channel);
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

        Serial.println("MIDI UART refreshed.");
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
