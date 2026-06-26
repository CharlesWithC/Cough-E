#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <inttypes.h>
#include <types.h>
#include <math.h>
#include <stdio.h>

static inline real_t floorreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::floor(x);
    #else
    return floorf((float)x);
    #endif
}

static inline real_t expreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::exp(x);
    #else
    return expf((float)x);
    #endif
}

static inline real_t powreal(real_t base, real_t exp){
    #ifdef UPOS_MODE
    return sw::universal::pow(base, exp);
    #else
    return powf((float)base, (float)exp);
    #endif
}

static inline real_t sqrtreal(real_t x){
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

static inline real_t logreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::log(x);
    #else
    return logf((float)x);
    #endif
}

static inline real_t log10real(real_t x){
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

/**
 * Divides all the elements of the input array by the same divisor.
 *
 * @param *x        : pointer to the input array
 * @param len       : length of the input array
 * @param divisor   : number to be used as a diviros for the elements of the array
*/
void vect_div_const(real_t *x, int16_t len, real_t divisor, real_t *res);


/**
 * Returns the sum of all the values of the input array.
 *
 * @param *x    : pointer to the input array
 * @param len   : lenght of the input array
*/
real_t vect_sum(const real_t *x, int16_t len);



/**
 * Returns the mean of a sequence of numbers

* @param *x    : pointer to the input array
* @param len   : lenght of the input array
 */
real_t vect_mean(const real_t *x, int16_t len);


/// @brief Computes the element-wise multiplication between x and y arrays,
/// the multiplication is stored in r array.
/// @param *x   pointer to the first vector
/// @param *y   pointer to the second vector
/// @param len  lenght of the vectors
/// @param *r   pointer to the resulting vectors
void vect_mult(real_t *x, const real_t *y, int16_t len, real_t *r);



/// @brief Returns the standard deviation of the given input array
/// @param *x       pointer to the signal
/// @param len      lenght of the signal
/// @return         the standard deviation
real_t vect_std(real_t *x, int16_t len);



/// @brief Copies "len" samples from "in" array of real_t starting at index "start"
/// Destination is "out"
/// Notice that the samples are taken from the input starting at
/// index "start" but are placed in output from index 0!
/// @param *in      pointer to the input signal
/// @param start    start index
/// @param len      lenght to copy
/// @param *out     poitner to the output array
void vect_copy(const real_t *in, int16_t start, int16_t len, real_t *out);



/// @brief Copies "len" samples from "in" array of uint16_t starting at index "start"
/// Destination is "out"
/// Notice that the samples are taken from the input starting at
/// index "start" but are placed in output from index 0!
/// @param *in      pointer to the input signal
/// @param start    start index
/// @param len      lenght to copy
/// @param *out     poitner to the output array
void vect_copy_uint16_t(uint16_t *in, int16_t start, int16_t len, uint16_t *out);



/// @brief Subtract a constant to each element of an input array and stores in into res array
/// @param *x           pointer to the input signal
/// @param len          lenght of the signal
/// @param constant     constant value to subtract
/// @param *res         pointer to the result
void sub_constant(const real_t *x, int16_t len, real_t constant, real_t *res);



/// @brief Returns the index at which the maximum value of array x is found
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @return     index of the maximum value
int16_t vect_max_index(real_t *x, int16_t len);



/// @brief Returns the maximum value within an array
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @return     max value
real_t vect_max_value(real_t *x, int16_t len);


/**
 * Returns the maximum absoulate value of the given array
 *
 * @param *x    :   pointer to the input array
 * @param len   :   length of the input array
 * @return      :   maximum absolute value in the input array
*/
real_t vect_max_abs_value(real_t *x, int16_t len);



/// @brief Divides every element in the specified "x" array by the maximum value.
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @param *res pointer to the resulting array
void normalize_max(real_t *x, int16_t len, real_t *res);



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
real_t get_line_length(real_t *x, int16_t len);



/// @brief Returns the kurtosis of the given input array
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the kurtosis
real_t get_kurtosis(real_t *x, int16_t len);



/// @brief Computes the L2 norm of a signal
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the L2 norm
real_t L2_norm(const real_t *x, int16_t len);



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
