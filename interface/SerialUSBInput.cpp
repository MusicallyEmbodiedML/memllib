#include "SerialUSBInput.hpp"
#include "../utils/SLIP.hpp"

#include "SerialUSBOutput.hpp"

SerialUSBOutput usbSerialOut;

SerialUSBInput::SerialUSBInput(std::shared_ptr<display> dispptr, size_t baud_rate) :
    slipBuffer{ 0 },
    value_states_{ 0 },
    spiState(SPISTATES::WAITFOREND),
    spiIdx(0),
    callback_(nullptr),
    refresh_uart_(false),
    baud_rate_(baud_rate)
{
    disp = dispptr;

    //assume Serial has already begun()
}

float bytes2float_union(const uint8_t* bytes) {
    union {
        uint32_t i;
        float f;
    } converter;
    
    converter.i = bytes[0] | 
                 (bytes[1] << 8) | 
                 (bytes[2] << 16) | 
                 (bytes[3] << 24);
    
    return converter.f;
}

void SerialUSBInput::Poll()
{
    if (!refresh_uart_) {
        // What baud rate is the UART running at?
        // Print debug info about PIO Serial state
        DEBUG_PRINT("PIO Serial Status - Initialized: ");
        DEBUG_PRINT(Serial ? "Yes" : "No");
        DEBUG_PRINT(", Target Baud Rate: ");
        DEBUG_PRINTLN(baud_rate_);
        // Start at current baud rate
        Serial1.begin(baud_rate_);
        DEBUG_PRINTLN("PIO_UART refreshed.");
        DEBUG_PRINT("Serial available: ");
        DEBUG_PRINTLN(Serial.available());
        refresh_uart_ = true;
    }

    while (true) {
        int raw = Serial.read();
        if (raw < 0) break;                           // no more data
        uint8_t spiByte = static_cast<uint8_t>(raw);

        switch(spiState) {
            case SPISTATES::WAITFOREND:
                if (spiByte == SLIP::END) {
                    slipBuffer[0] = SLIP::END;
                    spiState = SPISTATES::READBYTES;
                }
                break;

            // case SPISTATES::ENDORBYTES:
            //     if (spiByte == SLIP::END) {
            //         spiIdx = 1;
            //     } else {
            //         slipBuffer[1] = spiByte;
            //         spiIdx = 2;
            //     }
            //     spiState = SPISTATES::READBYTES;
            //     break;

            case SPISTATES::READBYTES:
                if (spiIdx < static_cast<int>(kSlipBufferSize_)) {
                    slipBuffer[spiIdx++] = spiByte;

                    if (spiByte == SLIP::END) {
                        // Safe decode into fixed buffer
                        disp->post("Received packet");
                        uint8_t outBuf[8] = {0};
                        size_t  maxOut = 8;
                        size_t  got    = SLIP::decode(slipBuffer, spiIdx, outBuf, maxOut);
                        if (got == maxOut) {
                            disp->post("Decoded packet successfully");
                            float f = bytes2float_union(outBuf);    
                            disp->post("Value: " + String(f, 8));

                            std::vector<float> values(1);
                            values[0] = f * 2.0f;
                            usbSerialOut.SendFloatArray(values);
                            disp->post("Sent value back: " + String(f, 8));
                            // spiMessage msg;
                            // memcpy(&msg, outBuf, maxOut);
                            // Parse_(msg);
                        }else{
                            disp->post("Failed to decode packet, got: " + String(got));
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

// void UARTInput::Parse_(spiMessage msg)
// {
//     static const float kEventThresh = 0.01;

//     // Find if this message's index is in our tracked indexes
//     auto it = std::find(sensor_indexes_.begin(), sensor_indexes_.end(), msg.msg);
//     if (it != sensor_indexes_.end()) {
//         size_t index = std::distance(sensor_indexes_.begin(), it);

//         // Protect against infs and nans
//         if (std::isnan(msg.value) || std::isinf(msg.value)) {
//             msg.value = value_states_[index];
//         }

//         float filtered_value = filters_[index].process(msg.value);
//         float prev_value = value_states_[index];

//         // Trigger callback whenever any value has changed
//         if (std::abs(filtered_value - prev_value) > kEventThresh) {
//             if (callback_) callback_(msg.msg, filtered_value);
//         }

//         // Print the value (Arduino scope) if it's the observed channel
//         if (kObservedChan == msg.msg) {
//             DEBUG_PRINT("Low:0.00,High:1.00,Value:");
//             DEBUG_PRINT(msg.value, 8);
//             DEBUG_PRINT(",FilteredValue:");
//             DEBUG_PRINTLN(filtered_value, 8);
//         }

//         value_states_[index] = filtered_value;
//     }
// }
