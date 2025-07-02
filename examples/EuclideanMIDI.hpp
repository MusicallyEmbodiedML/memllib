/**
 * @file EuclideanMIDI.hpp
 * @brief MIDI output for Euclidean rhythm generators
 *
 * @copyright Copyright (c) 2024. This Source Code Form is subject to the terms
 * of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __EUCLIDEAN_MIDI_HPP__
#define __EUCLIDEAN_MIDI_HPP__

#include <Arduino.h>
#include <vector>
#include <memory>
#include "../interface/MIDIInOut.hpp"
#include "EuclideanAudioApp.hpp"

/**
 * @brief MIDI note configuration structure
 *
 * Contains MIDI note number and channel for a single operator
 */
struct MIDINoteConfig {
    uint8_t note_number;  ///< MIDI note number (0-127)
    uint8_t channel;      ///< MIDI channel (1-16)

    /**
     * @brief Constructor with default values
     * @param note MIDI note number (default: 60 - middle C)
     * @param ch MIDI channel (default: 1)
     */
    MIDINoteConfig(uint8_t note = 60, uint8_t ch = 1) : note_number(note), channel(ch) {}
};

/**
 * @brief MIDI output class for Euclidean rhythm patterns
 *
 * Converts CV values from euclidean generators to MIDI note on/off messages.
 * Tracks note states to avoid duplicate messages and supports configurable
 * note numbers and channels for each operator.
 */
class EuclideanMIDI {
public:
    /**
     * @brief Maximum number of operators supported
     */
    static constexpr size_t kMaxOperators = EuclideanAudioApp::kN_Operators;

    /**
     * @brief Constructor
     */
    EuclideanMIDI();

    /**
     * @brief Destructor - ensures all notes are turned off
     */
    ~EuclideanMIDI();

    /**
     * @brief Setup the MIDI interface and note configuration
     *
     * @param midi_interface Shared pointer to MIDIInOut instance
     * @param note_configs Vector of MIDI note configurations (note number and channel) for each operator
     */
    void Setup(std::shared_ptr<MIDIInOut> midi_interface,
               const std::vector<MIDINoteConfig>& note_configs);

    /**
     * @brief Setup the MIDI interface with uniform channel
     *
     * @param midi_interface Shared pointer to MIDIInOut instance
     * @param note_numbers Vector of MIDI note numbers (0-127) for each operator
     * @param midi_channel MIDI channel for all notes (1-16, default: 1)
     */
    void Setup(std::shared_ptr<MIDIInOut> midi_interface,
               const std::vector<uint8_t>& note_numbers,
               uint8_t midi_channel = 1);

    /**
     * @brief Callback function compatible with euclidean_callback_t
     *
     * Processes CV values and sends corresponding MIDI note messages.
     * - CV > 0: Note ON with velocity proportional to CV value (scaled 0-1 to 0-127)
     * - CV = 0: Note OFF
     *
     * @param cv_values Vector of CV values from euclidean generators
     */
    void ProcessCV(const std::vector<float>& cv_values);

    /**
     * @brief Set MIDI note configurations for operators
     *
     * @param note_configs Vector of MIDI note configurations (note number and channel pairs)
     */
    void SetNoteNumbers(const std::vector<MIDINoteConfig>& note_configs);

    /**
     * @brief Set MIDI note numbers with uniform channel for operators
     *
     * @param note_numbers Vector of MIDI note numbers (0-127)
     * @param midi_channel MIDI channel for all notes (1-16, default: 1)
     */
    void SetNoteNumbers(const std::vector<uint8_t>& note_numbers, uint8_t midi_channel = 1);

    /**
     * @brief Get current note configurations
     *
     * @return Vector of current MIDI note configurations
     */
    const std::vector<MIDINoteConfig>& GetNoteConfigs() const { return note_configs_; }

    /**
     * @brief Turn off all currently active notes
     */
    void AllNotesOff();

    /**
     * @brief Check if MIDI interface is configured
     *
     * @return true if Setup() has been called successfully
     */
    bool IsConfigured() const { return midi_interface_ != nullptr; }

private:
    std::shared_ptr<MIDIInOut> midi_interface_;  ///< MIDI interface
    std::vector<MIDINoteConfig> note_configs_;   ///< MIDI note configurations for each operator
    std::vector<bool> note_states_;              ///< Current on/off state for each note
    std::vector<uint8_t> last_velocities_;       ///< Last sent velocity for each note

    /**
     * @brief Scale CV value (0-1) to MIDI velocity (0-127)
     *
     * @param cv_value CV input value
     * @return MIDI velocity value (0-127)
     */
    inline uint8_t ScaleCVToVelocity_(float cv_value) const {
        // Clamp CV value to [0, 1] range
        cv_value = cv_value < 0.0f ? 0.0f : (cv_value > 1.0f ? 1.0f : cv_value);
        return static_cast<uint8_t>(cv_value * 127.0f + 0.5f);
    }

    /**
     * @brief Send note on message with state tracking
     *
     * @param operator_index Index of the operator (0-based)
     * @param velocity MIDI velocity (1-127)
     */
    void SendNoteOn_(size_t operator_index, uint8_t velocity);

    /**
     * @brief Send note off message with state tracking
     *
     * @param operator_index Index of the operator (0-based)
     */
    void SendNoteOff_(size_t operator_index);
};

#endif // __EUCLIDEAN_MIDI_HPP__
