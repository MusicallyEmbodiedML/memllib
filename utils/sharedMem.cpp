#include "sharedMem.hpp"

namespace sharedMem {
#if OLD_LISTENING_MODE
    volatile float f0;
    volatile float f1;
    volatile float f2;
    volatile float f3;
#else
    volatile SaxAnalysis::parameters_t saxParams;
#endif
}
