#ifndef __PIOUART_HPP__
#define __PIOUART_HPP__

#include <Arduino.h>
#include <SerialPIO.h>
#include "../utils/MedianFilter.h"

#include <vector>
#include <functional>
#include <memory>


class PIOUART {

public:

    using piouart_callback_t = std::function<void(size_t, float)>;

    PIOUART(size_t n_extra_sensors,
            size_t sensor_rx,
            size_t sensor_tx,
            size_t baud_rate = 115200);

    // Ensure proper cleanup and non-copyable for shared_ptr safety
    ~PIOUART() = default;
    PIOUART(const PIOUART&) = delete;
    PIOUART& operator=(const PIOUART&) = delete;
    PIOUART(PIOUART&&) = default;
    PIOUART& operator=(PIOUART&&) = default;

    void Poll();
    inline void RegisterCallback(piouart_callback_t callback) {
        callback_ = callback;
    }

protected:
    static const size_t kSlipBufferSize_ = 128; // Increased buffer size
    static const size_t kMaxBytesPerPoll_ = 16; // Limit bytes processed per poll
    const size_t kNExtraSensors;
    uint8_t slipBuffer[kSlipBufferSize_];
    std::vector<MedianFilter<float>> filters_;
    std::vector<float> value_states_;
    piouart_callback_t callback_;

    struct spiMessage {
        uint8_t msg;
        float value;
    };
    enum SPISTATES {WAITFOREND, ENDORBYTES, READBYTES};
    SPISTATES spiState;
    size_t spiIdx; // Changed from int to size_t

    void Parse_(spiMessage msg);
    void ResetState_(); // Add reset function
};


#endif  // __PIOUART_HPP__
