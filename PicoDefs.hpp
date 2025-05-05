#ifndef __MEML_PICO_HPP__
#define __MEML_PICO_HPP__

#include "pico.h"

#define AUDIO_FUNC(x)    __not_in_flash_func(x)  ///< Macro to make audio function load from mem
#define AUDIO_MEM    __not_in_flash("audio")  ///< Macro to make variable load from mem
#define AUDIO_MEM_2  __not_in_flash("audio2")

#endif  // __MEML_PICO_HPP__
