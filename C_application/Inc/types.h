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
