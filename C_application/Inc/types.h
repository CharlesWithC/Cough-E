#ifndef TYPES_H
#define TYPES_H

#ifdef USE_UNUM_POSIT
    #ifndef __cplusplus
        #error "USE_UNUM_POSIT requires compiling with g++"
    #endif
    #include <universal/number/posit/posit.hpp>
    using namespace sw::universal;
    typedef posit<32, 2> num_t;
#else
    #ifdef USE_ASM_POSIT
        #error  "USE_ASM_POSIT is not implemented yet"
        // #include <libposit/posit.h>
        // typedef posit32_t num_t;
    #else
        typedef float num_t;
    #endif
#endif

#endif
