#include "UARTInput.hpp"
#include "../utils/SLIP.hpp"


UARTInput::UARTInput(size_t n_extra_sensors,
                 size_t sensor_rx,
                 size_t sensor_tx,
                 size_t baud_rate) :
    kNExtraSensors(n_extra_sensors),
    slipBuffer{ 0 },
    filters_(),
    value_states_{ 0 },
    spiState(SPISTATES::WAITFOREND),
    spiIdx(0),
    callback_(nullptr)
{
    filters_.resize(kNExtraSensors);
    value_states_.resize(kNExtraSensors);
    Serial2.setRX(sensor_rx);
    Serial2.setTX(sensor_tx);
    Serial2.begin(baud_rate);

    // Put all values in the middle
    for (auto &v : value_states_) {
        v = 0.5f;
    }
}

void UARTInput::Poll()
{
    uint8_t spiByte = 32;

    while (Serial2.available()) {
        spiByte = Serial2.read();

        switch(spiState) {
            case SPISTATES::WAITFOREND:
            //spiByte = Serial2.read();
            if (spiByte != -1) {
                if (spiByte == SLIP::END) {
                    // Serial.println("end");
                    slipBuffer[0] = SLIP::END;
                    spiState = SPISTATES::ENDORBYTES;
                }else{
                  // Serial.println(spiByte);
                }
            }
            break;
            case ENDORBYTES:
            //spiByte = Serial2.read();
            if (spiByte != -1) {
                if (spiByte == SLIP::END) {
                    //this is the message start
                    spiIdx = 1;

                }else{
                    slipBuffer[1] = spiByte;
                    spiIdx=2;
                }
                spiState = SPISTATES::READBYTES;
            }
            break;
            case READBYTES:
            //spiByte = Serial2.read();
            if (spiByte != -1) {

                slipBuffer[spiIdx++] = spiByte;
                if (spiByte == SLIP::END) {
                spiMessage decodeMsg;
                SLIP::decode(slipBuffer, spiIdx, reinterpret_cast<uint8_t*>(&decodeMsg));

                // Process the decoded message
                Parse_(decodeMsg);

                spiState = SPISTATES::WAITFOREND;
                }
            }
            break;
        }  // switch(spiState)

    }  // Serial2.available()

    if (spiIdx >= static_cast<int>(kSlipBufferSize_)) {
        Serial.println("UARTInput- Buffer overrun!!!");
    }
}

void UARTInput::Parse_(spiMessage msg)
{
    static const float kEventThresh = 0.01;

    if (msg.msg < value_states_.size()) {
        // Protect against infs and nans
        if (std::isnan(msg.value) || std::isinf(msg.value)) {
            msg.value = value_states_[msg.msg];
        }
        float filtered_value = filters_[msg.msg].process(msg.value);
        float prev_value = value_states_[msg.msg];
        // Trigger callback whenever any value has changed
        if (std::abs(filtered_value - prev_value) > kEventThresh) {
            callback_(value_states_);
        }
        // Print the value (Arduino scope) if it's the observed channel
        if (kObservedChan == msg.msg) {
            Serial.print("Low:0.00,High:1.00,Value:");
            Serial.print(msg.value, 8);
            Serial.print(",FilteredValue:");
            Serial.println(filtered_value, 8);
        }
        value_states_[msg.msg] = filtered_value;
    }
}
