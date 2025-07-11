#ifndef __MIDI_IN_OUT_HPP__
#define __MIDI_IN_OUT_HPP__

#include <Arduino.h>
#include <MIDI.h>
#include <memory>
#include "../hardware/memlnaut/Pins.hpp"
#include <functional>


class MIDIInOut
{
public:
    /**
     * @brief Constructor (only instantiates memory and member variables).     *
     */
    MIDIInOut();
    /**
     * @brief Sets up the MIDI interface on the required pins.
     * The CC numbers are set to the default values [0 .. (n_outputs-1)].
     *
     * @param n_outputs Number of output parameters that will be expected by SendParamsAsMIDICC().
     * @param midi_tx TX pin the MIDI device is connected to (default: Pins::MIDI_TX).
     * @param midi_rx RX pin the MIDI device is connected to (default: Pins::MIDI_RX).
     */
    void Setup(size_t n_outputs,
               bool midi_through = false,
               uint8_t midi_tx = Pins::MIDI_TX,
               uint8_t midi_rx = Pins::MIDI_RX);
    /**
     * @brief Set the MIDI channel to send messages on.
     *
     * @param channel MIDI channel (1-16).
     * @note The channel is set to 1-16, but the library uses 0-15 internally.
     */
    void SetMIDISendChannel(uint8_t channel);

    /**
     * @brief Set the MIDI channel to send note messages on.
     *
     * @param channel MIDI channel (1-16).
     * @note The channel is set to 1-16, but the library uses 0-15 internally.
     */
    void SetMIDINoteChannel(uint8_t channel);

    /**
     * @brief Send a MIDI Note On message
     *
     * @param note_number MIDI note number (0-127)
     * @param velocity Note velocity (0-127)
     * @return true if message sent successfully, false otherwise
     */
    bool sendNoteOn(uint8_t note_number, uint8_t velocity);

    /**
     * @brief Send a MIDI Note Off message
     *
     * @param note_number MIDI note number (0-127)
     * @param velocity Note velocity (0-127, typically 0)
     * @return true if message sent successfully, false otherwise
     */
    bool sendNoteOff(uint8_t note_number, uint8_t velocity = 0);

    /**
     * @brief Poll input. Put in a regular loop.
     */
    void Poll();
    /**
     * @brief Set the CC numbers that the parameters are sent to.
     *
     * @param cc_numbers Vector of MIDI CC numbers (0-127) to send the parameters to.
     * cc.numbers.size() must be equal to n_outputs.
     * @note The CC numbers are sent as MIDI messages, so they must be in the range 0-127.
     */
    void SetParamCCNumbers(const std::vector<uint8_t> &cc_numbers);
    /**
     * @brief Send the given vector of parameters as MIDI CC messages.
     *
     * @param params Vector of parameters to send as MIDI CC messages.
     * The size of the vector must be equal to n_outputs.
     * @note The parameters will be scaled from [0 .. 1] to [0 .. 127].
     */
    void SendParamsAsMIDICC(const std::vector<float> &params);

    /**
     * @typedef midi_cc_callback_t
     * @brief Function pointer type for MIDI CC callbacks
     *
     * @param cc_number The MIDI CC number (0-127)
     * @param cc_value The value of the MIDI CC message (0-127)
     *
     * This callback type is used for handling incoming MIDI Control Change messages.
     * The function takes two parameters: the CC number and its corresponding value,
     * both in the MIDI standard range of 0-127.
     */
    using midi_cc_callback_t = std::function<void(const uint8_t, const uint8_t)>;

    /**
     * @typedef midi_note_callback_t
     * @brief Function pointer type for MIDI note callbacks
     *
     * @param note_on True for note-on events, false for note-off events
     * @param note_number The MIDI note number (0-127)
     * @param velocity Note velocity/intensity (0-127)
     *
     * This callback type is used for handling incoming MIDI note messages.
     * The function takes three parameters: a boolean indicating note on/off status,
     * the note number, and velocity. Note number and velocity follow the MIDI
     * standard range of 0-127.
     */
    using midi_note_callback_t = std::function<void(const bool, const uint8_t, const uint8_t)>;

    /**
     * @brief Set the callback to be called when a MIDI CC message is received.
     *
     * @param callback Callback that accepts a CC number and value.
     */
    void SetCCCallback(midi_cc_callback_t callback);

    /**
     * @brief Set the callback to be called when a MIDI note message is received.
     *
     * @param callback Callback that accepts a note on/off status, note number, and velocity.
     */
    void SetNoteCallback(midi_note_callback_t callback);

    /**
     * @brief Structure for advanced CC parameter mapping
     */
    struct CCMapping {
        uint8_t cc_number;
        uint8_t channel;
        uint8_t min_value;
        uint8_t max_value;
        float scale_factor; // Precomputed for efficiency: (max - min)

        constexpr CCMapping() : cc_number(0), channel(1), min_value(0), max_value(127), scale_factor(127.0f) {}
        constexpr CCMapping(uint8_t cc, uint8_t ch, uint8_t min_val, uint8_t max_val)
            : cc_number(cc), channel(ch), min_value(min_val), max_value(max_val),
              scale_factor(max_val - min_val) {}
    };

    /**
     * @brief Set advanced parameter mappings with individual CC, channel, and range settings
     *
     * @param mappings Vector of CCMapping structures (must equal n_outputs_ size)
     */
    void SetAdvancedParamMappings(const std::vector<CCMapping>& mappings);

    /**
     * @brief Set mapping for a single parameter
     *
     * @param index Parameter index
     * @param cc_number CC number (0-127)
     * @param channel MIDI channel (1-16)
     * @param min_value Minimum output value (0-127)
     * @param max_value Maximum output value (0-127)
     */
    void SetParamMapping(size_t index, uint8_t cc_number, uint8_t channel, uint8_t min_value, uint8_t max_value);

    /**
     * @brief Clear advanced mappings and revert to simple mode
     */
    void ClearAdvancedMappings();

protected:
    std::vector<uint8_t> cc_numbers_;
    size_t n_outputs_;
    midi_cc_callback_t cc_callback_;
    midi_note_callback_t note_callback_;
    uint8_t send_channel_;  // Store the MIDI send channel (1-16)
    uint8_t note_channel_;  // Store the MIDI note channel (1-16)
    bool refresh_uart_;
    static MIDIInOut* instance_;  // Add static instance pointer

    // Advanced mapping support
    std::vector<CCMapping> advanced_mappings_;
    bool use_advanced_mappings_;

    // Helper for size mismatch warnings
    void warnSizeMismatch(const char* function_name, size_t expected, size_t actual) const;

private:
    // Static callback handlers
    static void handleControlChange(byte channel, byte number, byte value);
    static void handleNoteOn(byte channel, byte note, byte velocity);
    static void handleNoteOff(byte channel, byte note, byte velocity);
    void RefreshUART_(void);

    // Inline helper for efficient value scaling
    inline uint8_t scaleValue(float param, const CCMapping& mapping) const {
        float clamped = param > 1.0f ? 1.0f : (param < 0.0f ? 0.0f : param);
        return static_cast<uint8_t>(clamped * mapping.scale_factor + mapping.min_value + 0.5f);
    }
};

#endif  // __MIDI_IN_OUT_HPP__
