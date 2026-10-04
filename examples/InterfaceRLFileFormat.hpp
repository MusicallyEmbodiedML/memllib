#ifndef INTERFACE_RL_FILE_FORMAT_HPP
#define INTERFACE_RL_FILE_FORMAT_HPP

#include <stdint.h>

static constexpr uint16_t MEML_FILE_FORMAT_VERSION = 2;

// Binary file layout:
//   MEMLFileHeader  (26 bytes)
//   extra_size bytes of mode-specific data  (may be 0)
//   MLP binary (existing format)
//   version >= 2: the liked memories:
//     MEMLLikesHeader, then count x { float reward, float input[input_size],
//                                     float action[action_size] }
struct MEMLFileHeader {
    char     magic[4];       // always "MEML"
    uint16_t format_version; // MEML_FILE_FORMAT_VERSION
    char     mode_tag[16];   // null-padded mode identifier, e.g. "VerbFX"
    uint16_t extra_size;     // bytes of mode-specific data immediately following
};

struct MEMLLikesHeader {
    char     magic[4];       // always "LIKE"
    uint16_t count;
    uint16_t input_size;
    uint16_t action_size;
};

#endif // INTERFACE_RL_FILE_FORMAT_HPP
