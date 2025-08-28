#ifndef __AUDIO_DRIVER_HPP__
#define __AUDIO_DRIVER_HPP__

#include <Arduino.h>
#include <stddef.h>
#include "../PicoDefs.hpp"
#include "i2s_pio/i2s.h"

extern "C" {

const size_t kBufferSize = AUDIO_BUFFER_FRAMES;
constexpr size_t kSampleRate = 24000;
constexpr float kSampleRateRcpr = 1.0/kSampleRate;
const size_t kNChannels = 2;

typedef struct {
    float L;
    float R;
} stereosample_t;

using audiocallback_fptr_t = stereosample_t (*)(stereosample_t);
using audiocallback_block_fptr_t = void (*)(float[][kBufferSize], float[][kBufferSize], size_t, size_t);

}

extern audiocallback_fptr_t audio_callback_;
extern audiocallback_block_fptr_t audio_callback_block_;

enum PinConfig_i2c {
    i2c_sgt5000Data = 0,
    i2c_sgt5000Clk = 1,
    i2s_pDIN = 6,
    i2s_pDOUT = 7,
    i2s_pBCLK = 8,
    i2s_pWS = 9,
    i2s_pMCLK = 10,
};

extern volatile bool AUDIO_MEM dsp_overload;
extern float master_volume_;

class AudioDriver {
 public:
    typedef struct {
        bool mic_input;
        size_t line_level;
        size_t mic_gain_dB;
        float output_volume;
    } codec_config_t;
    static bool Setup();
    static bool Setup(const codec_config_t& config);
    static inline void SetCallback(audiocallback_fptr_t callback) {
        audio_callback_ = callback;
         DEBUG_PRINT("AUDIO_DRIVER - Callback address: ");
         DEBUG_PRINTF("%p\n", audio_callback_);
    }
    static inline void SetBlockCallback(audiocallback_block_fptr_t callback) {
        audio_callback_block_ = callback;
         DEBUG_PRINT("AUDIO_DRIVER - Block Callback address: ");
         DEBUG_PRINTF("%p\n", audio_callback_block_);
    }
    static inline void SetMasterVolume(float volume) {
        if (volume > 1.0f) {
            volume = 1.0f;
        } else if (volume < 0) {
            volume = 0;
        }
        master_volume_ = volume;
    }

    static inline size_t GetSampleRate() { return kSampleRate; }
    static size_t GetSysClockSpeed() {return 132000 * 2; } // 264MHz

    AudioDriver() = delete;

    static void i2sOutputCallback(void);
    static stereosample_t silence_(stereosample_t);

private:
    static void setDACVolume(float n);
};

#endif  // __AUDIO_DRIVER_HPP__
