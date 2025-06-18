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
    : midi_interface_(nullptr), midi_channel_(1) {
    // Initialize vectors with default values
    note_numbers_.resize(kMaxOperators, 60); // Default to middle C
    note_states_.resize(kMaxOperators, false);
    last_velocities_.resize(kMaxOperators, 0);
}

EuclideanMIDI::~EuclideanMIDI() {
    // Turn off all notes before destruction
    AllNotesOff();
}

void EuclideanMIDI::Setup(std::shared_ptr<MIDIInOut> midi_interface,
                          const std::vector<uint8_t>& note_numbers,
                          uint8_t midi_channel) {
    midi_interface_ = midi_interface;
    midi_channel_ = midi_channel;

    // Configure MIDI interface note channel
    if (midi_interface_) {
        midi_interface_->SetMIDINoteChannel(midi_channel_);
    }

    // Set note numbers
    SetNoteNumbers(note_numbers);

    Serial.print("EuclideanMIDI configured - Channel: ");
    Serial.print(midi_channel_);
    Serial.print(", Notes: ");
    for (size_t i = 0; i < note_numbers_.size() && i < kMaxOperators; ++i) {
        Serial.print(note_numbers_[i]);
        if (i < note_numbers_.size() - 1 && i < kMaxOperators - 1) {
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
    size_t num_operators = std::min(std::min(cv_values.size(), note_numbers_.size()), kMaxOperators);

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

void EuclideanMIDI::SetNoteNumbers(const std::vector<uint8_t>& note_numbers) {
    // Turn off all current notes before changing note numbers
    AllNotesOff();

    // Copy note numbers up to maximum operators
    size_t copy_count = std::min(note_numbers.size(), kMaxOperators);
    for (size_t i = 0; i < copy_count; ++i) {
        // Validate note number range
        if (note_numbers[i] <= 127) {
            note_numbers_[i] = note_numbers[i];
        } else {
            Serial.printf("Warning: Invalid note number %d for operator %d (using 60)\n",
                         note_numbers[i], i);
            note_numbers_[i] = 60; // Default to middle C
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

    // Turn off note first if it was already on (to avoid stuck notes)
    if (note_states_[operator_index]) {
        midi_interface_->sendNoteOff(note_numbers_[operator_index], 0);
    }

    // Send note on
    if (midi_interface_->sendNoteOn(note_numbers_[operator_index], velocity)) {
        note_states_[operator_index] = true;
        last_velocities_[operator_index] = velocity;
    }
}

void EuclideanMIDI::SendNoteOff_(size_t operator_index) {
    if (operator_index >= kMaxOperators || !IsConfigured()) {
        return;
    }

    if (midi_interface_->sendNoteOff(note_numbers_[operator_index], 0)) {
        note_states_[operator_index] = false;
        last_velocities_[operator_index] = 0;
    }
}
