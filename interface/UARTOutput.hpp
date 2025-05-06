#ifndef __UART_OUTPUT_HPP__
#define __UART_OUTPUT_HPP__

#include <Arduino.h>
#include <SerialPIO.h>

/**
 * @brief Sends float data to the Daisy using SLIP encoding over a PIO-based
 * "software" UART on a single GPIO pin (txPin).
 */
class UARTOutput
{
public:
    /**
     * @param txPin The GPIO pin on the Raspberry Pi Pico to use for TX.
     *              Must be a valid pin for PIO-based UART. 
     *              For example, "16" for GP16.
     */
    UARTOutput(int txPin);

    /**
     * @brief SLIP-encodes the float vector and sends it as one packet.
     * @param params  Vector of floats to send to the Daisy.
     */
    void SendParams(const std::vector<float> &params);

private:
    /**
     * @brief Helper to apply SLIP-escaping on a single byte.
     */
    void slipSendByte(uint8_t b);

private:
    // PIO-based Serial
    SerialPIO pioSerial_;

    // SLIP special characters
    static constexpr uint8_t SLIP_END     = 0xC0;  ///< End of SLIP packet
    static constexpr uint8_t SLIP_ESC     = 0xDB;  ///< Escape character
    static constexpr uint8_t SLIP_ESC_END = 0xDC;  ///< Escaped END
    static constexpr uint8_t SLIP_ESC_ESC = 0xDD;  ///< Escaped ESC
};

const size_t kN_synthparams = 3;

#endif  // __UART_OUTPUT_HPP__

