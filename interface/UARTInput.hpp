#ifndef __UART_INPUT_HPP__
#define __UART_INPUT_HPP__

#include <Arduino.h>
#include "../utils/MedianFilter.h"
#include "../hardware/memlnaut/Pins.hpp"
#include <SerialPIO.h>

#include <vector>

class UARTInput
{

public:
    // Change to trigger debugging of single channel
    static constexpr size_t kObservedChan = 9999;

    using uart_in_callback_t = void (*)(const std::vector<float> &);
    /**
     * @brief Construct a new UARTInput object for communication
     * with the MEML Sensor Board.
     *
     * @param sensor_indexes Vector of indexes to read (maximum 8, numbers 0-7).
     * @param sensor_rx RX pin the Sensor Board is connected to (default: Pins::SENSOR_RX).
     * @param sensor_tx TX pin the Sensor Board is connected to (default: Pins::SENSOR_TX).
     */
    UARTInput(const std::vector<size_t> &sensor_indexes,
              size_t sensor_rx = Pins::SENSOR_RX,
              size_t sensor_tx = Pins::SENSOR_TX,
              size_t baud_rate = 115200);
    /**
     * @brief Poll input. Put in a regular loop.
     */
    void Poll();
    /**
     * @brief Set the callback to be called when sensor data changes.
     *
     * @param callback Callback that accepts a vector of sensor readings.
     */
    inline void SetCallback(uart_in_callback_t callback)
    {
        callback_ = callback;
    }
    /**
     * @brief Change the sensor indexes to listen to.
     *
     * @param indexes Vector of indexes to read (maximum 8, numbers 0-7).
     */
    void ListenToSensorIndexes(const std::vector<size_t> &indexes);

protected:
    static const size_t kSlipBufferSize_ = 64;
    std::vector<size_t> sensor_indexes_;
    uint8_t slipBuffer[kSlipBufferSize_];
    std::vector<MedianFilter<float>> filters_;
    std::vector<float> value_states_;
    uart_in_callback_t callback_ = nullptr;

    struct spiMessage
    {
        uint8_t msg;
        float value;
    };
    enum SPISTATES
    {
        WAITFOREND,
        ENDORBYTES,
        READBYTES
    };
    SPISTATES spiState;
    int spiIdx;

    void Parse_(spiMessage msg);

private:
    SerialPIO pioSerial_;
};

#endif // __UART_INPUT_HPP__
