#ifndef __AUDIO_APP_BASE_HPP__
#define __AUDIO_APP_BASE_HPP__

#include "AudioDriver.hpp"
#include "../interface/InterfaceBase.hpp"
#include <functional>
#include <memory>
#include <vector>

class AudioAppBase {
protected:
    float sample_rate_;
    std::shared_ptr<InterfaceBase> interface_;
    static std::function<stereosample_t(stereosample_t)> callback_;

public:
    virtual AudioDriver::codec_config_t GetDriverConfig() const {
        return {
            .mic_input = false,
            .line_level = 3,
            .mic_gain_dB = 0,
            .output_volume = 0.97f
        };
    }

    AudioAppBase();
    virtual ~AudioAppBase();

    virtual stereosample_t Process(const stereosample_t x);
    static stereosample_t audioCallback(const stereosample_t x);
    virtual void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface);
    virtual void ProcessParams(const std::vector<float>& params);
    virtual void loop();
};

#endif // __AUDIO_APP_BASE_HPP__
