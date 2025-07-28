#ifndef SHAREDMEM_HPP
#define SHAREDMEM_HPP

#define OLD_LISTENING_MODE    0

#if !(OLD_LISTENING_MODE)
#include "../synth/SaxAnalysis.hpp"
#endif

// Add your shared memory related declarations and includes here

namespace sharedMem {
#if OLD_LISTENING_MODE
    extern volatile float f0;
    extern volatile float f1;
    extern volatile float f2;
    extern volatile float f3;
#else
    extern volatile SaxAnalysis::parameters_t saxParams;
#endif
}


#endif // SHAREDMEM_HPP