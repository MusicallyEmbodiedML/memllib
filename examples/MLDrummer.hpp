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
    static constexpr size_t kN_Params = 12;

 


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


        // dl1mix = smoothParams[0] * smoothParams[0] * 0.4f;
        // dl2mix = smoothParams[1] * smoothParams[1] * 0.4f;
        // dl3mix = smoothParams[2] * smoothParams[2] * 0.8f;
        // allp1fb = smoothParams[4] * 0.99f;
        // allp2fb = smoothParams[5] * 0.99f;
        // float comb1fb = (smoothParams[6] * 0.95f);
        // float comb2fb = (smoothParams[7] * 0.95f);

        // float dl1fb = (smoothParams[8] * 0.95f);
        // float dl2fb = (smoothParams[9] * 0.95f);
        // float dl3fb = (smoothParams[10] * 0.95f);


        // float y = dcb.play(mix, 0.99f) * 3.f;
        // float y1 = allp1.allpass(y, 30, allp1fb);
        // y1 = comb1.combfb(y1, 127, comb1fb);

        // float y2 = allp2.allpass(y, 500, allp2fb);
        // y2 = comb2.combfb(y2, 808, comb2fb);

        // y = y1 + y2;
        // float d1 = (dl1.play(y, 3500, dl1fb) * dl1mix);
        // float d2 = (dl2.play(y, 9000, dl2fb) * dl2mix);
        // float d3 = (dl3.play(y, 1199, dl3fb) * dl3mix);


        // y = y + d1 + d2 + d3;

        // y = tanhf(y);


        constexpr float segments= 32;
        constexpr float segLength = 1.f/segments;
        //cast phase with rounding
        float segmentRoot = static_cast<size_t>(smoothParams[0] * segments) * segLength;

        PERIODIC_DEBUG(5000,
            DEBUG_PRINTLN("bpf1: " + String(bpf1Val) + ", bpf2: " + String(bpf2Val) +
                          ", root: " + String(segmentRoot) );
        );

        // float y = sample_info.samples[static_cast<size_t>(phase+0.5f)];
        float y = sample_info.samples[static_cast<size_t>((segmentRoot * segLength) + segmentPhase+0.5f)];
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
        bpf1.set(maxiBiquad::filterTypes::BANDPASS, 100.f, 5.f, 0.f);
        bpf2.set(maxiBiquad::filterTypes::BANDPASS, 500.f, 5.f, 0.f);
        bpf3.set(maxiBiquad::filterTypes::BANDPASS, 1000.f, 5.f, 0.f);
        bpf4.set(maxiBiquad::filterTypes::BANDPASS, 4000.f, 5.f, 0.f);
    }

    void ProcessParams(const std::vector<float>& params) override
    {
        neuralNetOutputs = params;

    }

protected:

    maxiDelayline<5000> dl1;
    maxiDelayline<15100> dl2;
    maxiDelayline<1201> dl3;

    maxiReverbFilters<300> allp1;
    maxiReverbFilters<1000> allp2;
    maxiReverbFilters<300> comb1;
    maxiReverbFilters<1000> comb2;

    std::vector<float> neuralNetOutputs, smoothParams;


    float frame=0;
    float dl1mix = 0.0f;
    float dl2mix = 0.0f;
    float dl3mix = 0.0f;
    float allp1fb=0.5f;
    float allp2fb=0.5f;

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