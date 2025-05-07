#include "MIDI.hpp"

MIDI* MIDI::instance_ = nullptr;

MIDI::MIDI() : midi_(nullptr), n_outputs_(0), cc_callback_(nullptr), note_callback_(nullptr), send_channel_(1) {
    instance_ = this;
}

void MIDI::Setup(size_t n_outputs, uint8_t midi_tx, uint8_t midi_rx) {
    n_outputs_ = n_outputs;
    cc_numbers_.resize(n_outputs);
    // Initialize CC numbers to default values [0 .. (n_outputs-1)]
    static const size_t cc_start = 14;
    for (size_t i = 0; i < n_outputs; i++) {
        cc_numbers_[i] = static_cast<uint8_t>(i + cc_start);
    }

    // Setup Serial2 for MIDI
    Serial2.setTX(midi_tx);
    Serial2.setRX(midi_rx);

    // Create SerialMIDI object first
    serial_midi_ = std::make_unique<MIDI_NAMESPACE::SerialMIDI<HardwareSerial>>(Serial2);
    if (!serial_midi_) {
        Serial.println("Warning: Failed to create SerialMIDI interface");
        return;
    }

    // Now create MidiInterface with a reference to SerialMIDI
    midi_ = std::make_unique<MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial>>>(*serial_midi_);
    if (!midi_) {
        Serial.println("Warning: Failed to create MIDI interface");
        return;
    }

    midi_->begin(MIDI_CHANNEL_OMNI);

    // Setup static callbacks
    midi_->setHandleControlChange(handleControlChange);
    midi_->setHandleNoteOn(handleNoteOn);
    midi_->setHandleNoteOff(handleNoteOff);
}

void MIDI::Poll() {
    if (midi_ != nullptr) midi_->read();
}

void MIDI::SetParamCCNumbers(const std::vector<uint8_t>& cc_numbers) {
    if (cc_numbers.size() != n_outputs_) {
        Serial.printf("Warning: CC numbers vector size mismatch (expected %d, got %d)\n",
                     n_outputs_, cc_numbers.size());
        return;
    }
    cc_numbers_ = cc_numbers;
}

void MIDI::SendParamsAsMIDICC(const std::vector<float>& params) {
    if (!midi_) {
        Serial.println("Warning: MIDI interface not initialized");
        return;
    }
    if (params.size() != n_outputs_) {
        Serial.printf("Warning: Params vector size mismatch (expected %d, got %d)\n",
                     n_outputs_, params.size());
        return;
    }

    for (size_t i = 0; i < n_outputs_; i++) {
        uint8_t value = static_cast<uint8_t>(params[i] * 127.0f);
        midi_->sendControlChange(cc_numbers_[i], value, send_channel_);
    }
}

void MIDI::SetMIDISendChannel(uint8_t channel) {
    // Ensure channel is in valid range (1-16)
    if (channel < 1 || channel > 16) {
        Serial.printf("Warning: Invalid MIDI channel %d (must be 1-16)\n", channel);
        return;
    }
    send_channel_ = channel;
}

void MIDI::SetCCCallback(midi_cc_callback_t callback) {
    cc_callback_ = callback;
}

void MIDI::SetNoteCallback(midi_note_callback_t callback) {
    note_callback_ = callback;
}

// Static callback implementations
void MIDI::handleControlChange(byte channel, byte number, byte value) {
    if (instance_ && instance_->cc_callback_) {
        instance_->cc_callback_(number, value);
    }
}

void MIDI::handleNoteOn(byte channel, byte note, byte velocity) {
    if (instance_ && instance_->note_callback_) {
        instance_->note_callback_(true, note, velocity);
    }
}

void MIDI::handleNoteOff(byte channel, byte note, byte velocity) {
    if (instance_ && instance_->note_callback_) {
        instance_->note_callback_(false, note, velocity);
    }
}
