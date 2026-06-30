#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <range_analysis.h>
#include <types.h>

// Minimum real_t available (supposing 32-bit real_t)
#define MIN_FLOAT 1.17549e-038

// Costant used in the kurtosis computation using the
// Fisher definition
#define KURT_FISHER_CONST 3

template <typename T> static inline T floorreal(T x) {
    return floorf((float)x);
}
template <typename T> static inline T expreal(T x) { return expf((float)x); }
template <typename T, typename S> static inline T powreal(T base, S exp) {
    return powf((float)base, (float)exp);
}
template <typename T> static inline T sqrtreal(T x) { return sqrtf((float)x); }
template <typename T> static inline T logreal(T x) { return logf((float)x); }
template <typename T> static inline T log10real(T x) {
    return log10f((float)x);
}

#ifdef UPOS_MODE
template <typename T>
concept Universal = requires(T x) { T::nbits; };
template <Universal T> static inline T floorreal(T x) {
    return sw::universal::floor(x);
}
template <Universal T> static inline T expreal(T x) {
    return sw::universal::exp(x);
}
template <Universal T, typename S> static inline T powreal(T base, S exp) {
    return sw::universal::pow(base, static_cast<S>(exp));
}
template <Universal T> static inline T sqrtreal(T x) {
    return sw::universal::sqrt(x);
}
template <Universal T> static inline T logreal(T x) {
    return sw::universal::log(x);
}
template <Universal T> static inline T log10real(T x) {
    return sw::universal::log10(x);
}
#endif

#ifdef LPOS_MODE
static inline Posit sqrtreal(Posit x) { return libposit::sqrt(x); }
#endif

#ifdef DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#define DEBUG_PRINT_FLOAT(...) print_float(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) ((void)0)
#define DEBUG_PRINT_FLOAT(...) ((void)0)
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

#ifdef HEEPATIA_MODE
extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#include <w25q128jw.h>
#pragma GCC diagnostic pop
// need to manually declare this function when compiling with clang++/g++
uint32_t *heep_get_flash_address_offset(uint32_t *data_address_lma);
}
static inline void read_flash(const void *src, void *dest, uint32_t len) {
    uint32_t source_flash =
        (uint32_t)heep_get_flash_address_offset((uint32_t *)src);
    if (w25q128jw_read_standard(source_flash, dest, len) != FLASH_OK)
        printf("FLASH READ ERROR\n");
}
#else
#include <string.h>
static inline void read_flash(const void *src, void *dest, uint32_t len) {
    memcpy(dest, src, len);
}
#endif

/**
 * Enums of the available types on which to call the `order_by_idxs()` function.
 */
typedef enum type_sort { FLOAT_SORT, UINT16_T_SORT } type_sort_t;

// This serves as support for the argsort function
// It stores both values and indexes
typedef struct {
    uint16_t value;
    uint16_t idx;
} argsort_struct;

/*
    Set of general functions to help the computations of features
*/

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
template <RealType T>
static inline void _find_max(T *x, int16_t len, T *max_value,
                             int16_t *max_index) {
    T max_v = x[0];
    int16_t max_i = 0;
    for (int16_t i = 0; i < len; i++) {
        if (x[i] > max_v) {
            max_v = x[i];
            max_i = i;
        }
    }

    *max_index = max_i;
    *max_value = max_v;
}

/**
 * Divides all the elements of the input array by the same divisor.
 *
 * @param *x        : pointer to the input array
 * @param len       : length of the input array
 * @param divisor   : number to be used as a diviros for the elements of the
 * array
 */
template <RealType T>
static inline void vect_div_const(T *x, int16_t len, T divisor, T *res) {
    for (uint16_t i = 0; i < len; i++) {
        res[i] = x[i] / divisor;
    }
}

/**
 * Returns the sum of all the values of the input array.
 *
 * @param *x    : pointer to the input array
 * @param len   : lenght of the input array
 */
template <RealType T>
static inline T vect_sum(const T *x, int16_t len) {
    T sum = 0.0f;
    for (int16_t i = 0; i < len; i++) {
        sum += x[i];
    }
    return sum;
}

/**
 * Returns the mean of a sequence of numbers

* @param *x    : pointer to the input array
* @param len   : lenght of the input array
 */
template <RealType T>
static inline T vect_mean(const T *x, int16_t len) {
    return vect_sum(x, len) / len;
}

/// @brief Computes the element-wise multiplication between x and y arrays,
/// the multiplication is stored in r array.
/// @param *x   pointer to the first vector
/// @param *y   pointer to the second vector
/// @param len  lenght of the vectors
/// @param *r   pointer to the resulting vectors
template <RealType T>
static inline void vect_mult(T *x, const T *y, int16_t len, T *r) {
    for (int16_t i = 0; i < len; i++) {
        r[i] = x[i] * y[i];
    }
}

/// @brief Returns the standard deviation of the given input array
/// @param *x       pointer to the signal
/// @param len      lenght of the signal
/// @return         the standard deviation
template <RealType T>
static inline T vect_std(T *x, int16_t len) {
    T mean = vect_mean(x, len);
    T sum = 0.0;

    for (int16_t i = 0; i < len; i++) {
        T centered = x[i] - mean;
        RA_IMU_LOG_SCALAR("vect_std", "x_minus_mean", centered);
        T sq_dev = centered * centered;
        RA_IMU_LOG_SCALAR("vect_std", "sq_dev", sq_dev);
        sum += sq_dev;
    }

    RA_IMU_LOG_SCALAR("vect_std", "sum_sq_dev", sum);
    T variance = sum / len;
    RA_IMU_LOG_SCALAR("vect_std", "variance", variance);
    T result = sqrtreal(variance);
    RA_IMU_LOG_SCALAR("vect_std", "result", result);
    return result;
}

/// @brief Copies "len" samples from "in" array of real_t starting at index
/// "start" Destination is "out" Notice that the samples are taken from the
/// input starting at index "start" but are placed in output from index 0!
/// @param *in      pointer to the input signal
/// @param start    start index
/// @param len      lenght to copy
/// @param *out     poitner to the output array
template <RealType T, RealType S>
static inline void vect_copy(const T *in, int16_t start, int16_t len, S *out) {
    for (int16_t i = 0; i < len; i++) {
        out[i] = in[i + start];
    }
}

/// @brief Copies "len" samples from "in" array of uint16_t starting at index
/// "start" Destination is "out" Notice that the samples are taken from the
/// input starting at index "start" but are placed in output from index 0!
/// @param *in      pointer to the input signal
/// @param start    start index
/// @param len      lenght to copy
/// @param *out     poitner to the output array
static inline void vect_copy_uint16_t(uint16_t *in, int16_t start, int16_t len,
                                      uint16_t *out) {
    for (int16_t i = 0; i < len; i++) {
        out[i] = in[i + start];
    }
}

/// @brief Subtract a constant to each element of an input array and stores in
/// into res array
/// @param *x           pointer to the input signal
/// @param len          lenght of the signal
/// @param constant     constant value to subtract
/// @param *res         pointer to the result
template <RealType T>
static inline void sub_constant(const T *x, int16_t len, T constant, T *res) {
    for (int16_t i = 0; i < len; i++) {
        res[i] = x[i] - constant;
    }
}

/// @brief Returns the index at which the maximum value of array x is found
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @return     index of the maximum value
template <RealType T> static inline int16_t vect_max_index(T *x, int16_t len) {
    int16_t max_i = 0;
    T temp_v;
    _find_max(x, len, &temp_v, &max_i);
    return max_i;
}

/// @brief Returns the maximum value within an array
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @return     max value
template <RealType T> static inline T vect_max_value(T *x, int16_t len) {
    int16_t max_i = 0;
    T max_v;
    _find_max(x, len, &max_v, &max_i);
    return max_v;
}

/**
 * Returns the maximum absoulate value of the given array
 *
 * @param *x    :   pointer to the input array
 * @param len   :   length of the input array
 * @return      :   maximum absolute value in the input array
 */
template <RealType T> static inline T vect_max_abs_value(T *x, int16_t len) {
    T max_abs = x[0];
    T tmp = 0.0f;
    for (int16_t i = 1; i < len; i++) {
        tmp = fabs(x[i]);
        if (tmp >= max_abs) {
            max_abs = tmp;
        }
    }

    return max_abs;
}

/// @brief Divides every element in the specified "x" array by the maximum
/// value.
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @param *res pointer to the resulting array
template <RealType T>
static inline void normalize_max(T *x, int16_t len, T *res) {
    T max;
    int16_t max_i;
    _find_max(x, len, &max, &max_i);

    for (int16_t i = 0; i < len; i++) {
        res[i] = x[i] / max;
    }
}

/**
 * Comparator function to be passed to qsort() function.
 * It works with a structure having values and indexes, it has to be used for
 * the argsort implementation
 */
static inline int _q_argsort__cmp(const void *e1, const void *e2) {
    argsort_struct *val_1 = (argsort_struct *)e1;
    argsort_struct *val_2 = (argsort_struct *)e2;

    if ((*val_1).value > (*val_2).value) {
        return 1;
    }
    if ((*val_1).value < (*val_2).value) {
        return -1;
    }
    return 0;
}

/**
 * Computes the indexes that sort a given array.
 * Note that this function does't sort the initial array!
 *
 * @param *arr          :   pointer to the array to sort
 * @param len           :   length of the array
 * @param *sort_idxs    :   pointer to the array where to store the indexes that
 * would sort the `arr` array
 */
static inline void argsort(uint16_t *arr, uint16_t len, uint16_t *sort_idxs) {
    argsort_struct *elems =
        (argsort_struct *)malloc(len * sizeof(argsort_struct));

    for (uint16_t i = 0; i < len; i++) {
        elems[i].value = arr[i];
        elems[i].idx = i;
    }

    qsort(elems, len, sizeof(argsort_struct), _q_argsort__cmp);

    for (uint16_t i = 0; i < len; i++) {
        sort_idxs[i] = elems[i].idx;
    }

    free(elems);
}

/**
 * Orders an array based on some indexes specified as input parameters.
 * The array has to be of one of the availble types specified by the
 * `type_sort_t` enum, otherwise umpredictable behaviour may occour. Note that
 * the input array is ordered in-place, so its original content will be lost!
 *
 * @param *arr  :   pointer to the array to order
 * @param len   :   length of the array
 * @param *idxs :   pointer to the array of indexes that will order the input
 * array
 * @param type  :   type of the input array pointed by `arr`
 */
static inline void order_by_idxs(void *arr_in, uint16_t len, uint16_t *idxs,
                                 type_sort_t type) {
    if (type == FLOAT_SORT) {
        real_t *arr = (real_t *)arr_in;
        real_t *tmp = (real_t *)malloc(len * sizeof(real_t));

        for (uint16_t i = 0; i < len; i++) {
            // printf(">  %d\n", i);
            tmp[i] = arr[idxs[i]];
        }

        // Copies the idex-ordered tmp array back in place into arr
        vect_copy(tmp, 0, len, arr);

        free(tmp);
    } else if (type == UINT16_T_SORT) {
        uint16_t *arr = (uint16_t *)arr_in;
        uint16_t *tmp = (uint16_t *)malloc(len * sizeof(uint16_t));

        for (uint16_t i = 0; i < len; i++) {
            // printf(">  %d\n", i);
            tmp[i] = arr[idxs[i]];
        }

        // Copies the idex-ordered tmp array back in place into arr
        vect_copy_uint16_t(tmp, 0, len, arr);

        free(tmp);
    }
}

/// @brief Helper function that implements the definition of the composite
/// Simpson's integral. This function is not callable externally.
/// @param *x       pointer to the input signal
/// @param spacing  spacing
/// @param start    start index
/// @param end      end index
/// @return
template <RealType T>
static inline T _simpson_step(T *x, T spacing, int16_t start, int16_t end) {
    int n_intervals =
        (end - start) / 2; // realber of intervals (h in the formula)

    T sum = 0.0f;
    int interval_start = start;

    // computes the indexes
    for (int i = 0; i < n_intervals; i++) {
        sum += x[interval_start];
        sum += 4 * x[interval_start + 1];
        sum += x[interval_start + 2];
        interval_start = interval_start + 2;
    }
    return (sum * (spacing / 3));
}

/// @brief Returns the integral of signal x coputed using the composite
/// Simpson's rule. The spacing of the samples is defined by the
/// "spacing" parameter.
/// If the number of samples is even, the integral is averaged
/// @param *x   pointer to the inpug array
/// @param len  lenght of the array
/// @param spacing  spacing for the integral
/// @return integral of the signal
template <RealType T>
static inline T simpson(T *x, int16_t len, T spacing) {
    T result = 0.0f;

    if (len % 2 == 0) {
        T val = 0.0f;
        val += spacing * (x[len - 1] + x[len - 2]) / 2;
        result += _simpson_step(x, spacing, 0, len - 1);

        val += spacing * (x[0] + x[1]) / 2;
        result += _simpson_step(x, spacing, 1, len);

        val /= 2;
        result /= 2;
        result = result + val;
    } else {
        result = _simpson_step(x, spacing, 0, len);
    }
    return result;
}

/// @brief Applies a padding to the specified signal, it appends
/// "padlen" elements to the left and to the right.
/// The padded values are computed differently for the
/// left and the right ends.
/// @param *sig     pointer to the input signal
/// @param len      lenght of the signal
/// @param padlen   padding lenght
/// @param *res     pointer to the result
template <RealType T>
static inline void padding(const T *sig, int len, int padlen, T *res) {
    T left_end = sig[0];
    T right_end = sig[len - 1];

    T *left_ext = (T *)malloc(padlen * sizeof(T));
    T *right_ext = (T *)malloc(padlen * sizeof(T));

    // compute and append padding for the left side
    for (int i = 0; i < padlen; i++) {
        left_ext[i] = sig[padlen - i];
        right_ext[i] = sig[len - 2 - i];

        res[i] = (2 * left_end) - left_ext[i];
    }

    // copy the original signal in the central part of the result
    vect_copy(sig, 0, len, &res[padlen]);

    // computes and append padding for the right side
    for (int i = padlen + len; i < (padlen * 2) + len; i++) {
        res[i] = (2 * right_end) - right_ext[i - (padlen + len)];
    }

    free(left_ext);
    free(right_ext);
}

/// @brief Adds a 0 padding to the left and right of the input x
/// The amount of 0 added to each side is specified by side_pad_len
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @param side_pad_len lenght of the side padding
/// @param r            pointer to the result
template <RealType T>
static inline void zero_padding(const T *x, int16_t len, int16_t side_pad_len,
                                T *r) {
    // pad with zeros in front and in the end
    for (int16_t i = 0; i < side_pad_len; i++) {
        r[i] = 0.0f;
        r[i + side_pad_len + len] = 0.0f;
    }

    // copy input vector
    vect_copy(x, 0, len, &r[side_pad_len]);
}

/**
 * Pads the input array at the start and at the end by reflecting the first
 * `side_pad_len` numbers of the array (the first one excluded).
 *
 * @param *x: pointer to the input array to be padded
 * @param len: initial length of the input array
 * @param side_pad_len: length of the side paddings. Amount of numbers to add on
 * each side
 * @param *r: pointer to the result array
 */
template <RealType T>
static inline void reflect_padding(const T *x, uint16_t len,
                                   uint16_t side_pad_len, T *r) {
    // Adds the padding to head and tail of the array by reversing the original
    // samples
    for (uint16_t i = 0; i < side_pad_len; i++) {
        r[i] = x[side_pad_len - i];                 // head
        r[side_pad_len + len + i] = x[len - 2 - i]; // tail
    }

    // Copies the original array in the center of the result
    vect_copy(x, 0, len, &r[side_pad_len]);
}

/// @brief Computes the line length feature of the specified input array
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the line lenght
template <RealType T>
// T for input, optional S for output, optional Z for intermediate arithmetic
static inline T get_line_length(T *x, int16_t len) {
    T sum = 0.0f;

    for (int16_t i = 0; i < len - 1; i++) {
        sum += fabs(x[i + 1] - x[i]);
        RA_IMU_LOG_SCALAR("get_line_length", "diff", fabs(x[i + 1] - x[i]));
    }

    RA_IMU_LOG_SCALAR("get_line_length", "accum", sum);
    T result = sum / (len - 1);
    RA_IMU_LOG_SCALAR("get_line_length", "result", result);
    return result;
}

/// @brief Returns the kurtosis of the given input array
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the kurtosis
template <RealType T>
static inline T get_kurtosis(T *x, int16_t len) {
    T std = vect_std(x, len);
    T mean = vect_mean(x, len);

    RA_IMU_LOG_SCALAR("get_kurtosis", "mean", mean);
    RA_IMU_LOG_SCALAR("get_kurtosis", "std", std);

    T sum = 0.0f;
#ifdef RANGE_ANALYSIS
    T moment_max = 0.0f;
#endif

    for (int16_t i = 0; i < len; i++) {
        T tmp = (x[i] - mean) * (x[i] - mean);
        T x4 = tmp * tmp;
#ifdef RANGE_ANALYSIS
        if (x4 > moment_max)
            moment_max = x4;
#endif
        sum += x4;
    }

#ifdef RANGE_ANALYSIS
    RA_IMU_LOG_SCALAR("get_kurtosis", "moment_max", moment_max);
#endif
    RA_IMU_LOG_SCALAR("get_kurtosis", "sum_x4", sum);

    T std4 = powreal(std, 4);
    RA_IMU_LOG_SCALAR("get_kurtosis", "std4", std4);

    T result = (sum / (len * std4)) - KURT_FISHER_CONST;
    RA_IMU_LOG_SCALAR("get_kurtosis", "result", result);
    return result;
}

/// @brief Computes the L2 norm of a signal
/// @param *x           pointer to the inpug array
/// @param len          lenght of the array
/// @return     the L2 norm
template <RealType T>
static inline T L2_norm(const T *x, int16_t len) {
    T sum = 0.0f;

    for (int16_t i = 0; i < len; i++) {
        sum += x[i] * x[i];
    }

    RA_IMU_LOG_SCALAR("L2_norm", "sum_sq", sum);
    T result = sqrtreal(sum);
    RA_IMU_LOG_SCALAR("L2_norm", "result", result);
    return result;
}

/**
 * Returns the minimum of two `uint16_t` numbers.
 *
 * @param a     :   first number to compare
 * @param b     :   second number to compare
 */
static inline uint16_t min(uint16_t a, uint16_t b) {
    if (a < b)
        return a;
    else
        return b;
}

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
 * @param base  :   base of the logarithm. If base is 1, then it's the natural
 * log
 *
 */
template <RealType T>
static inline void entropy_calc(T *x, int16_t len, uint8_t base) {
    for (int16_t i = 0; i < len; i++) {
        if (x[i] > 0.0f) {
            x[i] = -1.0f * x[i] * logreal(x[i]);
        } else if (x[i] < 0.0f) {
            x[i] = MIN_FLOAT;
        }
    }

    // If base needed, change the base of the logarithm result
    if (base != 1) {
        for (int16_t i = 0; i < len; i++) {
            if (x[i] > MIN_FLOAT) {
                x[i] /= logreal((real_t)base);
            }
        }
    }
}

#endif
