#ifndef __MEML_PICO_HPP__
#define __MEML_PICO_HPP__

#include "pico.h"

#define AUDIO_FUNC(x)    __not_in_flash_func(x)  ///< Macro to make audio function load from mem
#define AUDIO_MEM    __not_in_flash("audio")  ///< Macro to make variable load from mem
#define AUDIO_MEM_2  __not_in_flash("audio2")
#define APP_SRAM __not_in_flash("app")

#define PERIODIC_DEBUG(COUNT, FUNC) \
        static size_t ct=0; \
        if (ct++ % COUNT == 0) { \
            FUNC         \
        }

// Add these macros near other globals
#define MEMORY_BARRIER() __sync_synchronize()
#define WRITE_VOLATILE(var, val) do { MEMORY_BARRIER(); (var) = (val); MEMORY_BARRIER(); } while (0)
#define READ_VOLATILE(var) ({ MEMORY_BARRIER(); typeof(var) __temp = (var); MEMORY_BARRIER(); __temp; })

#define ALLOW_DEBUG

#ifdef ALLOW_DEBUG
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__); Serial.flush()
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
#define DEBUG_PRINT(...)  
#define DEBUG_PRINTLN(...)  
#define DEBUG_PRINTF(...)  
#endif


#endif  // __MEML_PICO_HPP__
