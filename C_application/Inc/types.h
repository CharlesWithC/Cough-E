#ifndef TYPES_H
#define TYPES_H

#ifdef USE_UNUM_POSIT
    #ifndef __cplusplus
        #error "USE_UNUM_POSIT requires compiling with g++"
    #endif
    #include <universal/number/posit/posit.hpp>
    using namespace sw::universal;
    // use posit32 here, but posit24 seem to produce a similar result as float32
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

#ifdef LIMITED_MEMORY
#define FLASH __attribute__((section(".xheep_data_flash_only"))) __attribute__ ((aligned (16)))
#define GCRAM __attribute__((section(".xheep_data_gcram"))) __attribute__ ((aligned (16)))
#else
#define FLASH
#define GCRAM
#endif

#endif
