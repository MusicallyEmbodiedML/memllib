#ifndef __UART_INPUT_HPP__
#define __UART_INPUT_HPP__


#include <Arduino.h>
#include "../utils/MedianFilter.h"

#include <vector>


class UARTInput {

public:

    // Change to trigger debugging of single channel
    static constexpr size_t kObservedChan = 9999;

    using uart_in_callback_t = void (*)(const std::vector<float> &);
    UARTInput(size_t n_extra_sensors,
            size_t sensor_rx,
            size_t sensor_tx,
            size_t baud_rate = 115200);
    void Poll();
    inline void SetCallback(uart_in_callback_t callback) {
        callback_ = callback;
    }

protected:
    static const size_t kSlipBufferSize_ = 64;
    const size_t kNExtraSensors;
    uint8_t slipBuffer[kSlipBufferSize_];
    std::vector<MedianFilter<float>> filters_;
    std::vector<float> value_states_;
    uart_in_callback_t callback_ = nullptr;

    struct spiMessage {
    uint8_t msg;
    float value;
    };
    enum SPISTATES {WAITFOREND, ENDORBYTES, READBYTES};
    SPISTATES  spiState;
    int spiIdx;

    void Parse_(spiMessage msg);
};


#endif  // __UART_INPUT_HPP__
