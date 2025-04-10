/**
 * @file Pins.hpp
 * @brief Pin definitions and initialization for the FM Synth RL project
 * 
 * @copyright Copyright (c) 2024. This Source Code Form is subject to the terms
 * of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __PINS_HPP__
#define __PINS_HPP__
#include <Arduino.h>

/**
 * @brief Class containing all pin definitions and initialization functions
 */
class Pins {
public:
    /**
     * @name Momentary Switches
     * Digital inputs with internal pull-up resistors
     * @{
     */
    static constexpr uint8_t MOM_A1 = 24;  ///< Momentary switch A1
    static constexpr uint8_t MOM_A2 = 25;  ///< Momentary switch A2
    static constexpr uint8_t MOM_B1 = 26;  ///< Momentary switch B1
    static constexpr uint8_t MOM_B2 = 27;  ///< Momentary switch B2
    static constexpr uint8_t RE_SW = 23;   ///< Rotary encoder switch
    static constexpr uint8_t RE_B = 17;    ///< Rotary encoder B signal
    static constexpr uint8_t RE_A = 11;    ///< Rotary encoder A signal
    static constexpr uint8_t JOY_SW = 32;  ///< Joystick switch
    /** @} */

    /**
     * @name Toggle Switches
     * Digital inputs with internal pull-up resistors
     * @{
     */
    static constexpr uint8_t TOG_A1 = 28;  ///< Toggle switch A1
    static constexpr uint8_t TOG_A2 = 29;  ///< Toggle switch A2
    static constexpr uint8_t TOG_B1 = 30;  ///< Toggle switch B1
    static constexpr uint8_t TOG_B2 = 31;  ///< Toggle switch B2
    /** @} */

    /**
     * @name ADC Inputs
     * 12-bit resolution analog inputs
     * @{
     */
    static constexpr uint8_t JOY_X = 40;    ///< Joystick X-axis (ADC0)
    static constexpr uint8_t JOY_Y = 41;    ///< Joystick Y-axis (ADC1)
    static constexpr uint8_t JOY_Z = 42;    ///< Joystick Z-axis (ADC2)
    static constexpr uint8_t RV_GAIN1 = 47; ///< Gain potentiometer (ADC7)
    static constexpr uint8_t RV_Z1 = 46;    ///< Z1 potentiometer (ADC6)
    static constexpr uint8_t RV_Y1 = 45;    ///< Y1 potentiometer (ADC5)
    static constexpr uint8_t RV_X1 = 44;    ///< X1 potentiometer (ADC4)
    /** @} */

    /**
     * @name UART Pins
     * Digital pins used for UART communication
     * @{
     */
    static constexpr uint8_t DAISY_TX = 33; ///< One-way TX to Daisy
    static constexpr uint8_t SENSOR_TX = 36; ///< Sensor UART TX
    static constexpr uint8_t SENSOR_RX = 37; ///< Sensor UART RX
    /** @} */

    /**
     * @brief Initialize all pins with their respective modes
     */
    static void initializePins() {
        // Initialize momentary switches
        pinMode(MOM_A1, INPUT_PULLUP);
        pinMode(MOM_A2, INPUT_PULLUP);
        pinMode(MOM_B1, INPUT_PULLUP);
        pinMode(MOM_B2, INPUT_PULLUP);
        pinMode(RE_SW, INPUT_PULLUP);
        pinMode(RE_B, INPUT_PULLUP);
        pinMode(RE_A, INPUT_PULLUP);
        pinMode(JOY_SW, INPUT_PULLUP);

        // Initialize toggle switches
        pinMode(TOG_A1, INPUT_PULLUP);
        pinMode(TOG_A2, INPUT_PULLUP);
        pinMode(TOG_B1, INPUT_PULLUP);
        pinMode(TOG_B2, INPUT_PULLUP);

        // Initialize ADC pins
        pinMode(JOY_X, INPUT);
        pinMode(JOY_Y, INPUT);
        pinMode(JOY_Z, INPUT);
        pinMode(RV_GAIN1, INPUT);
        pinMode(RV_Z1, INPUT);
        pinMode(RV_Y1, INPUT);
        pinMode(RV_X1, INPUT);

        // Initialize UART pins (just as digital IO)
        pinMode(DAISY_TX, OUTPUT);
        pinMode(SENSOR_TX, OUTPUT);
        pinMode(SENSOR_RX, INPUT);

        // Set ADC resolution to 12 bits
        analogReadResolution(12);
    }
};

#endif // __PINS_HPP__
