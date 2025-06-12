#include "PIOUART.hpp"
#include "../utils/SLIP.hpp"


PIOUART::PIOUART(size_t n_extra_sensors,
                 size_t sensor_rx,
                 size_t sensor_tx,
                 size_t baud_rate) :
    kNExtraSensors(n_extra_sensors),
    slipBuffer{ 0 },
    filters_(),
    value_states_{ 0 },
    callback_(nullptr),
    spiState(SPISTATES::WAITFOREND),
    spiIdx(0)
{
    filters_.resize(kNExtraSensors);
    value_states_.resize(kNExtraSensors);

    Serial1.setTX(sensor_tx);
    Serial1.setRX(sensor_rx);
    Serial1.begin(baud_rate);

    // Put all values in the middle
    for (auto &v : value_states_) {
        v = 0.5f;
    }
}

void PIOUART::ResetState_() {
    spiState = SPISTATES::WAITFOREND;
    spiIdx = 0;
    memset(slipBuffer, 0, sizeof(slipBuffer));
}

void PIOUART::Poll()
{
    size_t bytesProcessed = 0;

    while (Serial1.available() && bytesProcessed < kMaxBytesPerPoll_) {
        uint8_t spiByte = Serial1.read();
        Serial.println(spiByte, HEX);
        bytesProcessed++;

        // Safety check for buffer overflow
        if (spiIdx >= kSlipBufferSize_ - 1) {
            Serial.println("PIOUART- Buffer overflow, resetting");
            ResetState_();
            continue;
        }

        // Debug: Check if we're seeing potential SLIP END characters
        if (spiByte == SLIP::END) {
            Serial.println("PIOUART- Saw SLIP END");
        } else if (spiByte == SLIP::ESC) {
            Serial.println("PIOUART- Saw SLIP ESC");
        } else if (spiByte == SLIP::ESC_END) {
            Serial.println("PIOUART- Saw SLIP ESC_END");
        } else if (spiByte == SLIP::ESC_ESC) {
            Serial.println("PIOUART- Saw SLIP ESC_ESC");
        }

        switch(spiState) {
            case SPISTATES::WAITFOREND:
                if (spiByte == SLIP::END) {
                    // Start of new frame - don't store the END byte
                    spiIdx = 0;
                    spiState = SPISTATES::READBYTES;
                    Serial.println("PIOUART- Frame start, reading data");
                }
                break;

            case SPISTATES::READBYTES:
                if (spiByte == SLIP::END) {
                    // End of frame - process if we have data
                    if (spiIdx > 0) {
                        Serial.printf("PIOUART- Frame complete with %zu data bytes\n", spiIdx);

                        // Print raw SLIP frame for debugging (data only, no END markers)
                        Serial.print("PIOUART- Raw SLIP data: ");
                        for (size_t i = 0; i < spiIdx; i++) {
                            Serial.printf("%02X ", slipBuffer[i]);
                        }
                        Serial.println();

                        uint8_t decodedBuffer[sizeof(spiMessage) + 4]; // Extra safety bytes
                        memset(decodedBuffer, 0, sizeof(decodedBuffer));

                        size_t decodedLength = SLIP::decode(slipBuffer, spiIdx, decodedBuffer);

                        Serial.printf("PIOUART- Decoded %zu bytes from %zu SLIP bytes (expected %zu)\n",
                                     decodedLength, spiIdx, sizeof(spiMessage));

                        if (decodedLength == sizeof(spiMessage)) {
                            spiMessage decodeMsg;
                            memcpy(&decodeMsg, decodedBuffer, sizeof(spiMessage));

                            // Validate message before parsing
                            if (decodeMsg.msg < kNExtraSensors &&
                                !std::isnan(decodeMsg.value) &&
                                !std::isinf(decodeMsg.value)) {
                                Parse_(decodeMsg);
                            } else {
                                Serial.printf("PIOUART- Invalid message: channel=%d, value=%f\n",
                                             decodeMsg.msg, decodeMsg.value);
                            }
                        } else {
                            Serial.printf("PIOUART- Size mismatch: expected %zu, got %zu\n",
                                         sizeof(spiMessage), decodedLength);
                            // Print raw decoded bytes for debugging
                            Serial.print("PIOUART- Raw decoded: ");
                            for (size_t i = 0; i < decodedLength && i < 16; i++) {
                                Serial.printf("%02X ", decodedBuffer[i]);
                            }
                            Serial.println();
                        }
                    } else {
                        Serial.println("PIOUART- Empty frame, ignoring");
                    }
                    ResetState_();
                } else {
                    // Data byte - store it
                    slipBuffer[spiIdx++] = spiByte;
                    Serial.printf("PIOUART- Data byte[%zu]: %02X\n", spiIdx-1, spiByte);
                }
                break;

            case SPISTATES::ENDORBYTES:
                // This state is no longer used - should not reach here
                Serial.println("PIOUART- ERROR: Unexpected ENDORBYTES state");
                ResetState_();
                break;
        }
    }
}

void PIOUART::Parse_(spiMessage msg)
{
    static const float kEventThresh = 0.01;
    static const size_t kObservedChan = 1;

    // Additional safety check (should not be needed if validation above works)
    if (msg.msg >= value_states_.size()) {
        Serial.printf("PIOUART- Invalid message channel in Parse_: %d\n", msg.msg);
        return;
    }

    if (msg.msg < value_states_.size()) {
        // Protect against infs and nans
        if (std::isnan(msg.value) || std::isinf(msg.value)) {
            msg.value = value_states_[msg.msg];
        }
        float filtered_value = filters_[msg.msg].process(msg.value);
        float prev_value = value_states_[msg.msg];
        if (std::abs(filtered_value - prev_value) > kEventThresh) {
            if (callback_) {
                callback_(msg.msg, filtered_value);
            }
        }
        if (kObservedChan == msg.msg) {
            Serial.print("Low:0.00,High:1.00,Value:");
            Serial.print(msg.value, 8);
            Serial.print(",FilteredValue:");
            Serial.println(filtered_value, 8);
        }
        value_states_[msg.msg] = filtered_value;
    } else {
        Serial.printf("PIOUART- Invalid message channel: %d\n", msg.msg);
    }
}
