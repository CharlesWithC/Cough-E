#ifdef UPOS_MODE // universal posit
    #ifndef __cplusplus
        #error "UPOS requires compiling with g++"
    #endif
    #include <universal/number/posit/posit.hpp>
    using namespace sw::universal;
    typedef posit<32, 2> real_t; // for anything not imu
    typedef posit<16, 2> bigreal_t; // for imu
    // yes bigreal has smaller size, thus 'mixed-posit-mess'
    // this is a experimental branch and the code is very messy
#else
#ifdef LPOS_MODE // libposit
    #ifndef __cplusplus
        #error "LPOS requires compiling with g++"
    #endif
    #define POSIT_SIZE 32
    #include <posit.hpp>
    using namespace libposit;
    typedef Posit real_t;
    typedef Posit bigreal_t;
#else
    typedef float real_t;
    typedef float bigreal_t;
#endif
#endif

#define POS Posit::from_bits

#ifdef HEEPATIA_MODE
#define DINTL0 __attribute__((section(".xheep_data_interleaved_sec0"))) __attribute__ ((aligned (4)))
#define DINTL1 __attribute__((section(".xheep_data_interleaved_sec1"))) __attribute__ ((aligned (4)))
#define FLASH0 __attribute__((section(".xheep_data_flash_only_sec0"))) __attribute__ ((aligned (4)))
#define FLASH1 __attribute__((section(".xheep_data_flash_only_sec1"))) __attribute__ ((aligned (4)))
#define FLASH2 __attribute__((section(".xheep_data_flash_only_sec2"))) __attribute__ ((aligned (4)))
#define FLASH3 __attribute__((section(".xheep_data_flash_only_sec3"))) __attribute__ ((aligned (4)))
#define CARUS00 __attribute__((section(".xheep_data_carus0_sec0"))) __attribute__ ((aligned (4)))
#define CARUS01 __attribute__((section(".xheep_data_carus0_sec1"))) __attribute__ ((aligned (4)))
#define CARUS10 __attribute__((section(".xheep_data_carus1_sec0"))) __attribute__ ((aligned (4)))
#define CARUS11 __attribute__((section(".xheep_data_carus1_sec1"))) __attribute__ ((aligned (4)))
#else
#define DINTL0
#define DINTL1
#define FLASH0
#define FLASH1
#define FLASH2
#define FLASH3
#define CARUS00
#define CARUS01
#define CARUS10
#define CARUS11
#endif
