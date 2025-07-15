#ifndef __XIASRIAUDIOAPP_HPP__
#define __XIASRIAUDIOAPP_HPP__

#include "../audio/AudioAppBase.hpp"
#include "../audio/AudioDriver.hpp"
#include "../../../sharedMem.hpp"
#include "../PicoDefs.hpp"
#include "../synth/OnePoleSmoother.hpp"
#include "../synth/maximilian.h"

// Flash memory address where audio data is loaded
#define AUDIO_FLASH_ADDRESS    0x10200000U
#define AUDIO_MAGIC            0x4F434950U  // 'PICO'
#define AUDIO_VERSION          1U

class MLDrummer : public AudioAppBase
{
public:

    static constexpr float segments = 16;
    static constexpr float segLength = 1.f/segments;

    static constexpr size_t kN_Params = segments;

    // Sample information structure
    typedef struct {
        const char* name;           // File name
        const float* samples;       // Pointer to audio samples
        uint32_t sample_count;      // Number of samples
        float duration;             // Duration in seconds
        bool found;                 // Whether the file was found
    } sample_info_t;

    // Binary format structures
    typedef struct {
        uint32_t magic;        // 'PICO' magic number
        uint32_t version;      // Format version
        uint32_t file_count;   // Number of audio files
        uint32_t sample_rate;  // Sample rate in Hz
    } audio_header_t;

    typedef struct {
        char name[16];         // Null-terminated filename
        uint32_t offset;       // Offset to audio data
        uint32_t sample_count; // Number of samples
        float duration;        // Duration in seconds
        uint32_t reserved;     // Reserved for future use
    } audio_file_entry_t;


    /**
     * Get sample information by filename (without .wav extension)
     *
     * @param filename Name of the file without .wav extension (e.g., "intro", "beep")
     * @param info Pointer to sample_info_t structure to fill
     * @return true if file found, false otherwise
     */
    bool get_sample_info(const char* filename, sample_info_t* info) {
        DEBUG_PRINTLN("get_sample_info called with filename: " + String(filename));
        if (!filename || !info) {
            return false;
        }

        // Initialize info structure
        memset(info, 0, sizeof(sample_info_t));

        // Read pointers from memory based on flash address
        const uint8_t* binary_data = (const uint8_t*)AUDIO_FLASH_ADDRESS;
        const audio_header_t* header = (const audio_header_t*)binary_data;
        const audio_file_entry_t* file_table = (const audio_file_entry_t*)(binary_data + 16);

        // Verify binary is valid
        if (header->magic != AUDIO_MAGIC) {
            return false;
        }

        // Search for the file by name
        for (uint32_t i = 0; i < header->file_count; i++) {
            if (strcmp(file_table[i].name, filename) == 0) {
                // Found the file!
                info->name = file_table[i].name;
                info->samples = (const float*)(binary_data + file_table[i].offset);
                info->sample_count = file_table[i].sample_count;
                info->duration = file_table[i].duration;
                info->found = true;
                return true;
            }
        }

        // File not found
        info->found = false;
        return false;
    }


    MLDrummer() : AudioAppBase(), smoother(150.f, kSampleRate),
        neuralNetOutputs(kN_Params, 0),
        smoothParams(kN_Params, 0)
     {
        if (!get_sample_info("afrfunk1", &sample_info)) {
            DEBUG_PRINTLN("Error: Sample  not found in audio data.");
        }else{
            DEBUG_PRINTLN("Sample found: " + String(sample_info.name) + ", count: " + String(sample_info.sample_count) + ", duration: " + String(sample_info.duration));
        }

     }




    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float mix = x.L + x.R;
        float bpf1Val = bpf1.play(mix) * 100.f;
        bpf1Val = bpfEnv1.play(bpf1Val);
        WRITE_VOLATILE(sharedMem::f0, bpf1Val);

        float bpf2Val = bpf2.play(mix) * 100.f;
        bpf2Val = bpfEnv2.play(bpf2Val);
        WRITE_VOLATILE(sharedMem::f1, bpf2Val);

        float bpf3Val = bpf3.play(mix) * 100.f;
        bpf3Val = bpfEnv3.play(bpf3Val);
        WRITE_VOLATILE(sharedMem::f2, bpf3Val);

        float bpf4Val = bpf4.play(mix) * 100.f;
        bpf4Val = bpfEnv4.play(bpf4Val);
        WRITE_VOLATILE(sharedMem::f3, bpf4Val);

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        // Process drum machine

        //cast phase with rounding
        size_t currentSegment = static_cast<size_t>(phase / sample_info.sample_count / segLength);
        float segmentRoot = (float)static_cast<size_t>(smoothParams[currentSegment] * segments);

        PERIODIC_DEBUG(44100,
            DEBUG_PRINTLN("bpf1: " + String(bpf1Val) + ", bpf2: " + String(bpf2Val) +
                          ", root: " + String(segmentRoot) + "seg: " + String(currentSegment)
                         );
        );

        // float y = sample_info.samples[static_cast<size_t>(phase+0.5f)];
        float y = sample_info.samples[static_cast<size_t>((segmentRoot * segLength * sample_info.sample_count) + segmentPhase+0.5f)];
        float rateInput = 1.f;
        // if (smoothParams[1] > 0.5f) {
        //     rateInput *= -1.f;
        // }
        phase += rateInput;
        segmentPhase += rateInput;

        float segLengthInSamples = segLength * sample_info.sample_count;
        if (rateInput >=0.f) {
            if (phase >= sample_info.sample_count) {
                phase -= sample_info.sample_count;
            }
            if (segmentPhase >= segLengthInSamples) {
                segmentPhase -= segLengthInSamples;
            }
        } else {
            if (phase < 0) {
                phase += sample_info.sample_count;
            }
            if (segmentPhase < 0) {
                segmentPhase += segLengthInSamples;
            }
        }

        stereosample_t ret { y, y };

        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        bpf1.set(maxiBiquad::filterTypes::BANDPASS, 200.f, 5.f, 0.f);
        bpf2.set(maxiBiquad::filterTypes::BANDPASS, 600.f, 5.f, 0.f);
        bpf3.set(maxiBiquad::filterTypes::BANDPASS, 1800.f, 5.f, 0.f);
        bpf4.set(maxiBiquad::filterTypes::BANDPASS, 5400.f, 5.f, 0.f);
    }

    void ProcessParams(const std::vector<float>& params) override
    {
        neuralNetOutputs = params;

    }

protected:

    std::vector<float> neuralNetOutputs, smoothParams;

    //listening
    maxiBiquad bpf1;
    maxiBiquad bpf2;
    maxiBiquad bpf3;
    maxiBiquad bpf4;

    maxiEnvelopeFollowerF bpfEnv1;
    maxiEnvelopeFollowerF bpfEnv2;
    maxiEnvelopeFollowerF bpfEnv3;
    maxiEnvelopeFollowerF bpfEnv4;

    maxiDCBlocker dcb;

    OnePoleSmoother<kN_Params> smoother;

    sample_info_t sample_info;
    float phase=0.f;
    float segmentPhase = 0.f;
    bool looping=true;
    bool playing = true;

};


#endif  // __XIASRIAUDIOAPP_HPP__