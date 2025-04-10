#ifndef __AUDIO_DRIVER_HPP__
#define __AUDIO_DRIVER_HPP__

#include <cstddef>
#include <Arduino.h>

extern "C" {

const size_t kBufferSize = 64;
constexpr size_t kSampleRate = 48000;
constexpr float kSampleRateRcpr = 1.0/kSampleRate;

typedef struct {
    float L;
    float R;
} stereosample_t;

using audiocallback_fptr_t = stereosample_t (*)(stereosample_t);

}

extern audiocallback_fptr_t audio_callback_;

enum PinConfig_i2c {
    i2c_sgt5000Data = 0,
    i2c_sgt5000Clk = 1,
    i2s_pDIN = 6,
    i2s_pDOUT = 7,
    i2s_pBCLK = 8,
    i2s_pWS = 9,
    i2s_pMCLK = 10,
};

class AudioDriver_Output {

 public:

    static bool Setup();
    static inline void SetCallback(audiocallback_fptr_t callback) {
        audio_callback_ = callback;
        Serial.print("AUDIO_DRIVER - Callback address: ");
        Serial.printf("%p\n", audio_callback_);
    }

    AudioDriver_Output() = delete;

    static void i2sOutputCallback(void);
    static stereosample_t silence_(stereosample_t);
};


#endif  // __AUDIO_DRIVER_HPP__
