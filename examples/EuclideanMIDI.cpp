/**
 * @file EuclideanMIDI.cpp
 * @brief MIDI output for Euclidean rhythm generators implementation
 *
 * @copyright Copyright (c) 2024. This Source Code Form is subject to the terms
 * of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "EuclideanMIDI.hpp"
#include <algorithm>

EuclideanMIDI::EuclideanMIDI()
    : midi_interface_(nullptr) {
    // Initialize vectors with default values
    note_configs_.resize(kMaxOperators, MIDINoteConfig(60, 1)); // Default to middle C on channel 1
    note_states_.resize(kMaxOperators, false);
    last_velocities_.resize(kMaxOperators, 0);
}

EuclideanMIDI::~EuclideanMIDI() {
    // Turn off all notes before destruction
    AllNotesOff();
}

void EuclideanMIDI::Setup(std::shared_ptr<MIDIInOut> midi_interface,
                          const std::vector<MIDINoteConfig>& note_configs) {
    midi_interface_ = midi_interface;

    // Set note configurations
    SetNoteNumbers(note_configs);

    Serial.print("EuclideanMIDI configured with individual channels - Notes: ");
    for (size_t i = 0; i < note_configs_.size() && i < kMaxOperators; ++i) {
        Serial.print(note_configs_[i].note_number);
        Serial.print("/ch");
        Serial.print(note_configs_[i].channel);
        if (i < note_configs_.size() - 1 && i < kMaxOperators - 1) {
            Serial.print(", ");
        }
    }
    Serial.println();
}

void EuclideanMIDI::Setup(std::shared_ptr<MIDIInOut> midi_interface,
                          const std::vector<uint8_t>& note_numbers,
                          uint8_t midi_channel) {
    midi_interface_ = midi_interface;

    // Configure MIDI interface note channel for backward compatibility
    if (midi_interface_) {
        midi_interface_->SetMIDINoteChannel(midi_channel);
    }

    // Set note numbers with uniform channel
    SetNoteNumbers(note_numbers, midi_channel);

    Serial.print("EuclideanMIDI configured - Channel: ");
    Serial.print(midi_channel);
    Serial.print(", Notes: ");
    for (size_t i = 0; i < note_configs_.size() && i < kMaxOperators; ++i) {
        Serial.print(note_configs_[i].note_number);
        if (i < note_configs_.size() - 1 && i < kMaxOperators - 1) {
            Serial.print(", ");
        }
    }
    Serial.println();
}

void EuclideanMIDI::ProcessCV(const std::vector<float>& cv_values) {
    if (!IsConfigured()) {
        return;
    }

    // Process CV values for each configured operator
    size_t num_operators = std::min(std::min(cv_values.size(), note_configs_.size()), kMaxOperators);

    for (size_t i = 0; i < num_operators; ++i) {
        float cv_value = cv_values[i];

        if (cv_value > 0.0f) {
            // Convert CV to velocity
            uint8_t velocity = ScaleCVToVelocity_(cv_value);

            // Ensure minimum velocity of 1 for note on
            if (velocity == 0) {
                velocity = 1;
            }

            // Send note on if not already on or velocity changed significantly
            if (!note_states_[i] || abs(static_cast<int>(velocity) - static_cast<int>(last_velocities_[i])) > 5) {
                SendNoteOn_(i, velocity);
            }
        } else {
            // CV value is 0 or negative - send note off
            if (note_states_[i]) {
                SendNoteOff_(i);
            }
        }
    }
}

void EuclideanMIDI::SetNoteNumbers(const std::vector<MIDINoteConfig>& note_configs) {
    // Turn off all current notes before changing note numbers
    AllNotesOff();

    // Copy note configurations up to maximum operators
    size_t copy_count = std::min(note_configs.size(), kMaxOperators);
    for (size_t i = 0; i < copy_count; ++i) {
        // Validate note number and channel ranges
        if (note_configs[i].note_number <= 127 &&
            note_configs[i].channel >= 1 && note_configs[i].channel <= 16) {
            note_configs_[i] = note_configs[i];
        } else {
            Serial.printf("Warning: Invalid note config %d/ch%d for operator %d (using 60/ch1)\n",
                         note_configs[i].note_number, note_configs[i].channel, i);
            note_configs_[i] = MIDINoteConfig(60, 1); // Default to middle C on channel 1
        }
    }

    // Reset all states
    std::fill(note_states_.begin(), note_states_.end(), false);
    std::fill(last_velocities_.begin(), last_velocities_.end(), 0);
}

void EuclideanMIDI::SetNoteNumbers(const std::vector<uint8_t>& note_numbers, uint8_t midi_channel) {
    // Turn off all current notes before changing note numbers
    AllNotesOff();

    // Copy note numbers up to maximum operators with uniform channel
    size_t copy_count = std::min(note_numbers.size(), kMaxOperators);
    for (size_t i = 0; i < copy_count; ++i) {
        // Validate note number range
        if (note_numbers[i] <= 127 && midi_channel >= 1 && midi_channel <= 16) {
            note_configs_[i] = MIDINoteConfig(note_numbers[i], midi_channel);
        } else {
            Serial.printf("Warning: Invalid note number %d or channel %d for operator %d (using 60/ch1)\n",
                         note_numbers[i], midi_channel, i);
            note_configs_[i] = MIDINoteConfig(60, 1); // Default to middle C on channel 1
        }
    }

    // Reset all states
    std::fill(note_states_.begin(), note_states_.end(), false);
    std::fill(last_velocities_.begin(), last_velocities_.end(), 0);
}

void EuclideanMIDI::AllNotesOff() {
    if (!IsConfigured()) {
        return;
    }

    for (size_t i = 0; i < kMaxOperators; ++i) {
        if (note_states_[i]) {
            SendNoteOff_(i);
        }
    }
}

void EuclideanMIDI::SendNoteOn_(size_t operator_index, uint8_t velocity) {
    if (operator_index >= kMaxOperators || !IsConfigured()) {
        return;
    }

    const MIDINoteConfig& config = note_configs_[operator_index];

    // Set the MIDI channel for this note
    midi_interface_->SetMIDINoteChannel(config.channel);

    // Turn off note first if it was already on (to avoid stuck notes)
    if (note_states_[operator_index]) {
        midi_interface_->sendNoteOff(config.note_number, 0);
    }

    // Send note on
    if (midi_interface_->sendNoteOn(config.note_number, velocity)) {
        note_states_[operator_index] = true;
        last_velocities_[operator_index] = velocity;
    }
}

void EuclideanMIDI::SendNoteOff_(size_t operator_index) {
    if (operator_index >= kMaxOperators || !IsConfigured()) {
        return;
    }

    const MIDINoteConfig& config = note_configs_[operator_index];

    // Set the MIDI channel for this note
    midi_interface_->SetMIDINoteChannel(config.channel);

    if (midi_interface_->sendNoteOff(config.note_number, 0)) {
        note_states_[operator_index] = false;
        last_velocities_[operator_index] = 0;
    }
}
