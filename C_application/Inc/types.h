#ifdef UPOS_MODE // universal posit
    #ifndef __cplusplus
        #error "UPOS requires compiling with g++"
    #endif
    #include <universal/number/posit/posit.hpp>
    using namespace sw::universal;
    typedef posit<32, 2> real_t;
#else
#ifdef LPOS_MODE // libposit
    #ifndef __cplusplus
        #error "LPOS requires compiling with g++"
    #endif
    #include <libposit/posit.hpp>
    typedef Posit real_t;
#else
    typedef float real_t;
#endif
#endif

#ifdef HEEPATIA_MODE
#define DINTL __attribute__((section(".xheep_data_interleaved")))
#define FLASH __attribute__((section(".xheep_data_flash_only"))) __attribute__ ((aligned (16)))
#define GCRAM __attribute__((section(".xheep_rodata_virtgcram"))) __attribute__ ((aligned (16)))
#define CARUS0 __attribute__((section(".xheep_data_carus0"))) __attribute__ ((aligned (16)))
#define CARUS1 __attribute__((section(".xheep_data_carus1"))) __attribute__ ((aligned (16)))
#else
#define DINTL
#define FLASH
#define GCRAM
#define CARUS0
#define CARUS1
#endif
