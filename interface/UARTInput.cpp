#include "UARTInput.hpp"
#include "../utils/SLIP.hpp"


UARTInput::UARTInput(const std::vector<size_t>& sensor_indexes,
                 size_t sensor_rx,
                 size_t sensor_tx,
                 size_t baud_rate) :
    sensor_indexes_(sensor_indexes),
    slipBuffer{ 0 },
    filters_(),
    value_states_{ 0 },
    spiState(SPISTATES::WAITFOREND),
    spiIdx(0),
    callback_(nullptr)
{
    filters_.resize(sensor_indexes.size());
    value_states_.resize(sensor_indexes.size());
    Serial2.setRX(sensor_rx);
    Serial2.setTX(sensor_tx);
    Serial2.begin(baud_rate);

    // Put all values in the middle
    for (auto &v : value_states_) {
        v = 0.5f;
    }
}

void UARTInput::ListenToSensorIndexes(const std::vector<size_t>& indexes)
{
    sensor_indexes_ = indexes;

    // Clear and recreate filters
    filters_.clear();
    filters_.resize(indexes.size());

    // Clear and reset value states
    value_states_.clear();
    value_states_.resize(indexes.size(), 0.5f);
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

    // Find if this message's index is in our tracked indexes
    auto it = std::find(sensor_indexes_.begin(), sensor_indexes_.end(), msg.msg);
    if (it != sensor_indexes_.end()) {
        size_t index = std::distance(sensor_indexes_.begin(), it);

        // Protect against infs and nans
        if (std::isnan(msg.value) || std::isinf(msg.value)) {
            msg.value = value_states_[index];
        }

        float filtered_value = filters_[index].process(msg.value);
        float prev_value = value_states_[index];

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

        value_states_[index] = filtered_value;
    }
}
