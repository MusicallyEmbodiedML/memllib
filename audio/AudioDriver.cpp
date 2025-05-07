#include "AudioDriver.hpp"
#include <Arduino.h>
#include "Wire.h"

#include "control_sgtl5000.h"
#include "i2s_pio/i2s.h"
#include "../synth/maximilian.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "../PicoDefs.hpp"


#define TEST_TONES    0
#define PASSTHROUGH   0


constexpr int sampleRate = kSampleRate; // minimum for many i2s DACs
constexpr int bitsPerSample = 32;

constexpr float amplitude = 1 << (bitsPerSample - 2); // amplitude of square wave = 1/2 of maximum
constexpr float neg_amplitude = -amplitude; // amplitude of square wave = 1/2 of maximum
constexpr float one_over_amplitude = 1.f / amplitude;

constexpr int32_t sample = amplitude; // current sample value

static AUDIO_MEM_2 AudioControlSGTL5000 codecCtl;

audiocallback_fptr_t audio_callback_ = nullptr;

static __attribute__((aligned(8))) pio_i2s __scratch_y("audio") i2s;


#if TEST_TONES
maxiOsc osc, osc2;

float f1=20, f2=2000;
#endif  // TEST_TONES

float __force_inline _scale_and_saturate(float x) {
    x *= amplitude;
    if (x > amplitude) {
        x = amplitude;
    } else if (x < neg_amplitude) {
        x = neg_amplitude;
    }
    return x;
}

float __force_inline _scale_down(float x) {
    return x * one_over_amplitude;
}


void __force_inline process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    // Timing start
    auto ts = micros();

    for (size_t i = 0; i < num_frames; i++) {
        
        const size_t indexL = i << 1;
        const size_t indexR = indexL + 1;

        stereosample_t y { 
            _scale_down(static_cast<float>(input[indexL])),
            _scale_down(static_cast<float>(input[indexR]))
        };

#if !(TEST_TONES)

#if !(PASSTHROUGH)
        y = audio_callback_(y);

        stereosample_t y_scaled {
            _scale_and_saturate(y.L),
            _scale_and_saturate(y.R),
        };
        output[indexL] = static_cast<int32_t>(y_scaled.L);
        output[indexR] = static_cast<int32_t>(y_scaled.R);

#else

        // output[i] = input[i];

        output[i*2] = input[i*2];
         output[(i*2) + 1] = input[(i*2) + 1];
#endif  // PASSTHROUGH

#else

        output[i*2] = osc.sinewave(f1) * sample;
        output[(i*2) + 1] = osc2.sinewave(f2) * sample;
        // count++;
        f1 *= 1.00001;
        f2 *= 1.00001;
        if (f1 > 15000) {
          f1 = 20.0;
        }
        if (f2 > 15000) {
          f2 = 20.0;
        }
#endif  // TEST_TONES

    }
    // Timing end
    auto elapsed = micros() - ts;
    static constexpr float quantumLength = 1.f/
            ((static_cast<float>(kBufferSize)/static_cast<float>(kSampleRate))
            * 1000000.f);
    const float dspload = elapsed * quantumLength;
    // Serial.println(dspload);
    // Report DSP overload if needed
    static volatile bool AUDIO_MEM dsp_overload = false;
    if (dspload > 0.95 and !dsp_overload) {
        dsp_overload = true;
    } else if (dspload < 0.9) {
        dsp_overload = false;
    }
}

void __isr dma_i2s_in_handler(void) {
    /* We're double buffering using chained TCBs. By checking which buffer the
     * DMA is currently reading from, we can identify which buffer it has just
     * finished reading (the completion of which has triggered this interrupt).
     */
    if (*(int32_t**)dma_hw->ch[i2s.dma_ch_in_ctrl].read_addr == i2s.input_buffer) {
        // It is inputting to the second buffer so we can overwrite the first
        process_audio(i2s.input_buffer, i2s.output_buffer, AUDIO_BUFFER_FRAMES);
    } else {
        // It is currently inputting the first buffer, so we write to the second
        process_audio(&i2s.input_buffer[STEREO_BUFFER_SIZE], &i2s.output_buffer[STEREO_BUFFER_SIZE], AUDIO_BUFFER_FRAMES);
    }
    dma_hw->ints0 = 1u << i2s.dma_ch_in_data;  // clear the IRQ
}


void __isr AudioDriver::i2sOutputCallback() {


    // for(size_t i=0;  i < kBufferSize; i++) {

    //     stereosample_t y { 0 };
    //     y = audio_callback_(y);

    //     stereosample_t y_scaled {
    //         _scale_and_saturate(y.L),
    //         _scale_and_saturate(y.R),
    //     };
    //     i2s.write32(static_cast<int32_t>(y_scaled.L), static_cast<int32_t>(y_scaled.R));
    // }

    // Timing end
    // auto elapsed = micros() - ts;
    // static constexpr float quantumLength = 1.f/
    //         ((static_cast<float>(kBufferSize)/static_cast<float>(kSampleRate))
    //         * 1000000.f);
    // float dspload = elapsed * quantumLength;
    // // Report DSP overload if needed
    // static volatile bool dsp_overload = false;
    // if (dspload > 0.95 and !dsp_overload) {
    //     dsp_overload = true;
    // } else if (dspload < 0.9) {
    //     dsp_overload = false;
    // }
}

bool AudioDriver::Setup() {

    if (nullptr == audio_callback_) {
        audio_callback_ = &silence_;
    }

    maxiSettings::setup(kSampleRate, 2, kBufferSize);

    if (!Wire.setSDA(i2c_sgt5000Data) ||
            !Wire.setSCL(i2c_sgt5000Clk)) {
        Serial.println("AUDIO- Failed to setup I2C with codec!");
    }

    set_sys_clock_khz(132000*2, true);
    // set_sys_clock_khz(129600, true);
    Serial.printf("System Clock: %lu\n", clock_get_hz(clk_sys));

    i2s_config picoI2SConfig {
        kSampleRate, // 48000,
        256,
        bitsPerSample, // 32,
        i2s_pMCLK, // 10,
        i2s_pDIN,  // 6,
        i2s_pDOUT,  // 7,
        i2s_pBCLK, // 8,
        true
    };
    i2s_program_start_synched(pio0, &picoI2SConfig, dma_i2s_in_handler, &i2s);

    // init i2c
    codecCtl.enable();
    codecCtl.volume(0.8);
    codecCtl.inputSelect(AUDIO_INPUT_LINEIN);
    codecCtl.lineInLevel(3);


    return true;
}


stereosample_t AudioDriver::silence_(stereosample_t x) {
    x.L = 0;
    x.R = 0;

    return x;
}

//breaking change
void AudioDriver::setDACVolume(float n) {
    codecCtl.dacVolume(n);
}
