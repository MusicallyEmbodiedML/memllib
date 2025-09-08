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
    callback_(nullptr),
    refresh_uart_(false),
    baud_rate_(baud_rate)
{
    // Reserve once to avoid heap fragmentation at runtime
    filters_.reserve(sensor_indexes.size());
    value_states_.reserve(sensor_indexes.size());
    filters_.resize(sensor_indexes.size());
    value_states_.resize(sensor_indexes.size(), 0.5f);

    // Tie RX to a known level to avoid floating
    pinMode(sensor_rx, INPUT_PULLDOWN);

    Serial1.setTX(sensor_tx);
    Serial1.setRX(sensor_rx);
    Serial1.begin(baud_rate);
}

void UARTInput::ListenToSensorIndexes(const std::vector<size_t>& indexes)
{
    sensor_indexes_ = indexes;

    // Reserve to avoid repeated reallocs
    filters_.reserve(indexes.size());
    value_states_.reserve(indexes.size());

    filters_.clear();
    filters_.resize(indexes.size());
    value_states_.clear();
    value_states_.resize(indexes.size(), 0.5f);
}

void UARTInput::Poll()
{
    if (!refresh_uart_) {
        // What baud rate is the UART running at?
        // Print debug info about PIO Serial state
        DEBUG_PRINT("PIO Serial Status - Initialized: ");
        DEBUG_PRINT(Serial1 ? "Yes" : "No");
        DEBUG_PRINT(", Target Baud Rate: ");
        DEBUG_PRINTLN(baud_rate_);
        // Start at current baud rate
        Serial1.begin(baud_rate_);
        DEBUG_PRINTLN("PIO_UART refreshed.");
        DEBUG_PRINT("Serial available: ");
        DEBUG_PRINTLN(Serial1.available());
        refresh_uart_ = true;
    }

    while (true) {
        int raw = Serial1.read();
        if (raw < 0) break;                           // no more data
        uint8_t spiByte = static_cast<uint8_t>(raw);

        switch(spiState) {
            case SPISTATES::WAITFOREND:
                if (spiByte == SLIP::END) {
                    slipBuffer[0] = SLIP::END;
                    spiState = SPISTATES::ENDORBYTES;
                }
                break;

            case SPISTATES::ENDORBYTES:
                if (spiByte == SLIP::END) {
                    spiIdx = 1;
                } else {
                    slipBuffer[1] = spiByte;
                    spiIdx = 2;
                }
                spiState = SPISTATES::READBYTES;
                break;

            case SPISTATES::READBYTES:
                if (spiIdx < static_cast<int>(kSlipBufferSize_)) {
                    slipBuffer[spiIdx++] = spiByte;

                    if (spiByte == SLIP::END) {
                        // Safe decode into fixed buffer
                        uint8_t outBuf[sizeof(spiMessage)] = {0};
                        size_t  maxOut = sizeof(spiMessage);
                        size_t  got    = SLIP::decode(slipBuffer, spiIdx, outBuf, maxOut);
                        if (got == maxOut) {
                            spiMessage msg;
                            memcpy(&msg, outBuf, maxOut);
                            Parse_(msg);
                        }
                        // Reset for next packet
                        spiState = SPISTATES::WAITFOREND;
                        spiIdx   = 0;
                    }
                } else {
                    DEBUG_PRINTLN("UARTInput: SLIP buffer overrun, dropping packet");
                    spiState = SPISTATES::WAITFOREND;
                    spiIdx   = 0;
                }
                break;
        }
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
            if (callback_) callback_(msg.msg, filtered_value);
        }

        // Print the value (Arduino scope) if it's the observed channel
        if (kObservedChan == msg.msg) {
            DEBUG_PRINT("Low:0.00,High:1.00,Value:");
            DEBUG_PRINT(msg.value, 8);
            DEBUG_PRINT(",FilteredValue:");
            DEBUG_PRINTLN(filtered_value, 8);
        }

        value_states_[index] = filtered_value;
    }
}
