#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <inttypes.h>
#include <types.h>
#include <math.h>
#include <stdio.h>

template <typename T>
concept IsRealType = std::same_as<T, real_t> || std::same_as<T, bigreal_t>;

template <IsRealType T>
static inline T floorreal(T x){
    #ifdef UPOS_MODE
    return sw::universal::floor(x);
    #else
    return floorf((float)x);
    #endif
}

template <IsRealType T>
static inline T expreal(T x){
    #ifdef UPOS_MODE
    return sw::universal::exp(x);
    #else
    return expf((float)x);
    #endif
}

template <IsRealType T>
static inline T powreal(T base, T exp){
    #ifdef UPOS_MODE
    return sw::universal::pow(base, exp);
    #else
    return powf((float)base, (float)exp);
    #endif
}

template <IsRealType T>
static inline T sqrtreal(T x){
    #ifdef UPOS_MODE
    return sw::universal::sqrt(x);
    #else
    #ifdef LPOS_MODE
    return libposit::sqrt(x);
    #else
    return sqrtf((float)x);
    #endif
    #endif
}

template <IsRealType T>
static inline T logreal(T x){
    #ifdef UPOS_MODE
    return sw::universal::log(x);
    #else
    return logf((float)x);
    #endif
}

template <IsRealType T>
static inline T log10real(T x){
    #ifdef UPOS_MODE
    return sw::universal::log10(x);
    #else
    return log10f((float)x);
    #endif
}

#ifdef HEEPATIA_MODE
#ifdef __cplusplus
extern "C" {
#endif
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wc++11-narrowing"
    #include <w25q128jw.h>
    #pragma GCC diagnostic pop
    // need to manually declare this function when compiling with clang++/g++
    uint32_t * heep_get_flash_address_offset(uint32_t* data_address_lma);
#ifdef __cplusplus
}
#endif

static inline void read_flash(const void *src, void *dest, uint32_t len) {
    uint32_t source_flash = (uint32_t)heep_get_flash_address_offset((uint32_t *)src);
    if(w25q128jw_read_standard(source_flash, dest, len) != FLASH_OK) printf("FLASH READ ERROR\n");
}
#else
#include <string.h>
static inline void read_flash(const void *src, void *dest, uint32_t len) {
    memcpy(dest, src, len);
}
#endif

static inline void print_float(float f, int precision) {
    if (f < 0) {
        printf("-");
        f = -f;
    }

    int ipart = (int)f;
    float fpart = f - (float)ipart;
    int multiplier = 1;
    for (int i = 0; i < precision; i++) {
        multiplier *= 10;
    }
    int fpart_int = (int)(fpart * multiplier + 0.5f);
    if (fpart_int >= multiplier) {
        ipart += 1;
        fpart_int = 0;
    }
    printf("%d.", ipart);
    int temp = fpart_int;
    int digits_printed = 0;
    if (temp == 0) {
        digits_printed = 1;
    } else {
        while (temp > 0) {
            digits_printed++;
            temp /= 10;
        }
    }
    for (int i = 0; i < (precision - digits_printed); i++) {
        printf("0");
    }
    printf("%d", fpart_int);
}

/**
 * Enums of the available types on which to call the `order_by_idxs()` function.
*/
typedef enum type_sort{
    FLOAT_SORT,
    UINT16_T_SORT
} type_sort_t;

/*
    Set of general functions to help the computations of features
*/

/**
 * Computes the indexes that sort a given array.
 * Note that this function does't sort the initial array!
 *
 * @param *arr          :   pointer to the array to sort
 * @param len           :   length of the array
 * @param *sort_idxs    :   pointer to the array where to store the indexes that would sort the `arr` array
*/
void argsort(uint16_t *arr, uint16_t len, uint16_t *sort_idxs);

/**
 * Orders an array based on some indexes specified as input parameters.
 * The array has to be of one of the availble types specified by the `type_sort_t` enum, otherwise umpredictable behaviour may occour.
 * Note that the input array is ordered in-place, so its original content will be lost!
 *
 * @param *arr  :   pointer to the array to order
 * @param len   :   length of the array
 * @param *idxs :   pointer to the array of indexes that will order the input array
 * @param type  :   type of the input array pointed by `arr`
*/
void order_by_idxs(void *arr, uint16_t len, uint16_t *idxs, type_sort_t type);




/*
    Helper function for the maximum value and relative index computation
    It stores to max_value and max_index the maximum value and relative index
    found in the array x, respectively
*/

/// @brief Helper function for the maximum value and relative index computation
/// It stores to max_value and max_index the maximum value and relative index
/// found in the array x, respectively
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @param *max_value   max value
/// @param *max_index   index of the max value
template <typename T>
static inline void _find_max(T *x, int16_t len, T *max_value, int16_t *max_index){
    T max_v = x[0];
    int16_t max_i = 0;
    for(int16_t i=0; i<len; i++){
        if(x[i] > max_v){
            max_v = x[i];
            max_i = i;
        }
    }

    *max_index = max_i;
    *max_value = max_v;
}


template <typename T>
static inline void vect_div_const(T *x, int16_t len, T divisor, T *res){
    for(uint16_t i=0; i<len; i++){
        res[i] = x[i] / divisor;
    }
}


template <typename T>
static inline T vect_sum(const T *x, int16_t len){
    T sum = 0.0;
    for(int16_t i=0; i<len; i++){
        sum += x[i];
    }
    return sum;
}

template <typename T>
static inline T vect_mean(const T *x, int16_t len){
    return vect_sum(x, len) / len;
}

template <typename T>
static inline void vect_mult(T *x, const T *y, int16_t len, T *r){
    for(int16_t i=0; i<len; i++){
        r[i] = x[i] * y[i];
    }
}


template <typename T>
static inline T vect_std(T *x, int16_t len){
    T mean = vect_mean(x, len);
    T sum = 0.0;

    for(int16_t i=0; i<len; i++){
        T centered = x[i] - mean;
        //RA_IMU_LOG_SCALAR("vect_std", "x_minus_mean", centered);
        T sq_dev = centered * centered;
        //RA_IMU_LOG_SCALAR("vect_std", "sq_dev", sq_dev);
        sum += sq_dev;
    }

    //RA_IMU_LOG_SCALAR("vect_std", "sum_sq_dev", sum);
    T variance = sum / len;
    //RA_IMU_LOG_SCALAR("vect_std", "variance", variance);
    T result = sqrtreal(variance);
    //RA_IMU_LOG_SCALAR("vect_std", "result", result);
    return result;
}


template <typename T>
static inline void vect_copy(const T *in, int16_t start, int16_t len, T *out){
    for(int16_t i=0; i<len; i++){
        out[i] = in[i + start];
    }
}




static inline void vect_copy_uint16_t(uint16_t *in, int16_t start, int16_t len, uint16_t *out){
    for(int16_t i=0; i<len; i++){
        out[i] = in[i + start];
    }
}


template <typename T>
static inline void sub_constant(const T *x, int16_t len, T constant, T *res){
    for(int16_t i=0; i<len; i++){
        res[i] = x[i] - constant;
    }
}


template <typename T>
static inline int16_t vect_max_index(T *x, int16_t len){
    int16_t max_i = 0.0;
    T temp_v;
    _find_max(x, len, &temp_v, &max_i);
    return max_i;
}


template <typename T>
static inline T vect_max_value(T *x, int16_t len){
    int16_t max_i = 0;
    T max_v;
    _find_max(x, len, &max_v, &max_i);
    return max_v;
}


template <typename T>
static inline T vect_max_abs_value(T *x, int16_t len){
    T max_abs = x[0];
    T tmp = 0.0;
    for(int16_t i=1; i<len; i++){
        tmp = fabs(x[i]);
        if(tmp >= max_abs){
            max_abs = tmp;
        }
    }

    return max_abs;
}



template <typename T>
static inline void normalize_max(T *x, int16_t len, T *res){
    T max;
    int16_t max_i;
    _find_max(x, len, &max, &max_i);

    for(int16_t i=0; i<len; i++){
        res[i] = x[i] / max;
    }
}



/// @brief Returns the integral of signal x coputed using the composite
/// Simpson's rule. The spacing of the samples is defined by the
/// "spacing" parameter.
/// If the number of samples is even, the integral is averaged
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @param spacing  spacing for the integral
/// @return integral of the signal
real_t simpson(real_t *x, int16_t len, real_t spacing);



/// @brief Applies a padding to the specified signal, it appends
/// "padlen" elements to the left and to the right.
/// The padded values are computed differently for the
/// left and the right ends.
/// @param *sig     pointer to the input signal
/// @param len      lenght of the signal
/// @param padlen   padding lenght
/// @param *res     pointer to the result
void padding(const real_t *sig, int len, int padlen, real_t *res);



/// @brief Adds a 0 padding to the left and right of the input x
/// The amount of 0 added to each side is specified by side_pad_len
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @param side_pad_len lenght of the side padding
/// @param r            pointer to the result
void zero_padding(const real_t *x, int16_t len, int16_t side_pad_len, real_t *r);



/**
 * Pads the input array at the start and at the end by reflecting the first `side_pad_len`
 * numbers of the array (the first one excluded).
 *
 * @param *x: pointer to the input array to be padded
 * @param len: initial length of the input array
 * @param side_pad_len: length of the side paddings. Amount of numbers to add on each side
 * @param *r: pointer to the result array
*/
void reflect_padding(const real_t *x, uint16_t len, uint16_t side_pad_len, real_t *r);



/// @brief Computes the line length feature of the specified input array
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the line lenght
bigreal_t get_line_length(bigreal_t *x, int16_t len);



/// @brief Returns the kurtosis of the given input array
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the kurtosis
bigreal_t get_kurtosis(bigreal_t *x, int16_t len);



/// @brief Computes the L2 norm of a signal
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the L2 norm
bigreal_t L2_norm(const bigreal_t *x, int16_t len);



/**
 * Returns the minimum of two `uint16_t` numbers.
 *
 * @param a     :   first number to compare
 * @param b     :   second number to compare
*/
uint16_t min(uint16_t a, uint16_t b);



/**
 * Computes the entropy of the input array with the following formula:
 *                   | -xlog(x)    if x > 0.0
 *      entropy(x) = | 0.0         if x = 0.0
 *                   | -inf        if x < 0.0
 *
 * Notice that the input array is modified in-place, so its content will be
 * changed by the function.
 *
 * @param *x    :   pointer to the input array
 * @param len   :   length of the input array
 * @param base  :   base of the logarithm. If base is 1, then it's the natural log
 *
*/
void entropy_calc(real_t *x, int16_t len, uint8_t base);

#endif
