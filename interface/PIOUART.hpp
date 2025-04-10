#ifndef __PIOUART_HPP__
#define __PIOUART_HPP__


#include <Arduino.h>
#include "../utils/MedianFilter.h"

#include <vector>


class PIOUART {

 public:
    PIOUART(size_t n_extra_sensors,
            size_t sensor_rx,
            size_t sensor_tx,
            size_t baud_rate = 115200);
    void Poll();

 protected:
    static const size_t kSlipBufferSize_ = 64;
    const size_t kNExtraSensors;
    uint8_t slipBuffer[kSlipBufferSize_];
    std::vector<MedianFilter<float>> filters_;
    std::vector<float> value_states_;

    struct spiMessage {
    uint8_t msg;
    float value;
    };
    enum SPISTATES {WAITFOREND, ENDORBYTES, READBYTES};
    SPISTATES  spiState;
    int spiIdx;

    void Parse_(spiMessage msg);
};


#endif  // __PIOUART_HPP__
