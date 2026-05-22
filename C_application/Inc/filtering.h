#ifndef _FILTERING_H_
#define _FILTERING_H_

#include "types.h"

/**
 * Applies a linear filter to the signal. The filter is defined by a, b, and zi coefficients
 *
 * @param *sig  : pointer to the signal to be filtered
 * @param len   : lenght of the signal
 * @param *b    : pointer to the b coefficients of the filter
 * @param *a    : pointer to the a coefficients of the filter
 * @param *zi   : pointer to the zi coefficients of the filter
 * @param *res  : pointer to the array where to store the resulting filtered signal
 */
void linear_filer(num_t *sig, int len, const num_t *b, const num_t *a, num_t *zi, num_t *res);


/**
 *  Applies a filter to the signal.
    The filter has transfer function with coefficients b and a, passed as parameters
    The way this filter is implemented is compliant with the filtfilt() function
    of the python package "scipy".

    b, a and zi are respectively the b, a coefficients of the filter and
    its initial state.

    The main steps are:
    - padding of the signal
    - forward filtering
    - backward filtering
 */
void filtfilt(const num_t *sig, int len, const num_t *b, const num_t* a, const num_t *zi, num_t *res);

#endif
