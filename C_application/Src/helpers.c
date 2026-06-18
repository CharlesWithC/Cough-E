#include <stdlib.h>
#include <math.h>
#include <types.h>

#include <helpers.h>

#include <range_analysis.h>

// Minimum real_t available (supposing 32-bit real_t)
#define MIN_FLOAT 1.17549e-038

// Costant used in the kurtosis computation using the
// Fisher definition
#define KURT_FISHER_CONST   3

real_t expreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::exp(x);
    #else
    return expf(x);
    #endif
}

real_t sqrtreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::sqrt(x);
    #else
    return sqrtf(x);
    #endif
}

real_t logreal(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::log(x);
    #else
    return logf(x);
    #endif
}

real_t log10real(real_t x){
    #ifdef UPOS_MODE
    return sw::universal::log10(x);
    #else
    return log10f(x);
    #endif
}

// Internal functions to support some computations
void _find_max(real_t *x, int16_t len, real_t *max_value, int16_t *max_index);
real_t _simpson_step(real_t *x, real_t spacing, int16_t start, int16_t end);


// This serves as support for the argsort function
// It stores both values and indexes
typedef struct {
    uint16_t value;
    uint16_t idx;
} argsort_struct;

/**
 * Comparator function to be passed to qsort() function.
 * It works with a structure having values and indexes, it has to be used for the argsort implementation
*/
int _q_argsort__cmp(const void *e1, const void *e2){

    argsort_struct *val_1 = (argsort_struct*)e1;
    argsort_struct *val_2 = (argsort_struct*)e2;

    if( (*val_1).value > (*val_2).value ){
        return 1;
    }
    if( (*val_1).value < (*val_2).value ){
        return -1;
    }
    return 0;
}


void argsort(uint16_t *arr, uint16_t len, uint16_t *sort_idxs){

    argsort_struct *elems = (argsort_struct*)malloc(len * sizeof(argsort_struct));

    for(uint16_t i=0; i<len; i++){
        elems[i].value = arr[i];
        elems[i].idx = i;
    }

    qsort(elems, len, sizeof(argsort_struct), _q_argsort__cmp);

    for(uint16_t i=0; i<len; i++){
        sort_idxs[i] = elems[i].idx;
    }

    free(elems);

}


void order_by_idxs(void *arr_in, uint16_t len, uint16_t *idxs, type_sort_t type){

    if(type == FLOAT_SORT){

        real_t *arr = (real_t*)arr_in;
        real_t *tmp = (real_t*)malloc(len * sizeof(real_t));

        for(uint16_t i=0; i<len; i++){
            // printf(">  %d\n", i);
            tmp[i] = arr[idxs[i]];
        }

        // Copies the idex-ordered tmp array back in place into arr
        vect_copy(tmp, 0, len, arr);

        free(tmp);

    } else if(type == UINT16_T_SORT) {

        uint16_t *arr = (uint16_t*)arr_in;
        uint16_t *tmp = (uint16_t*)malloc(len * sizeof(uint16_t));

        for(uint16_t i=0; i<len; i++){
            // printf(">  %d\n", i);
            tmp[i] = arr[idxs[i]];
        }

        // Copies the idex-ordered tmp array back in place into arr
        vect_copy_uint16_t(tmp, 0, len, arr);

        free(tmp);
    }
}



void vect_div_const(real_t *x, int16_t len, real_t divisor, real_t *res){
    for(uint16_t i=0; i<len; i++){
        res[i] = x[i] / divisor;
    }
}



real_t vect_sum(const real_t *x, int16_t len){
    real_t sum = 0.0;
    for(int16_t i=0; i<len; i++){
        sum += x[i];
    }
    return sum;
}


real_t vect_mean(const real_t *x, int16_t len){
    return vect_sum(x, len) / len;
}


void vect_mult(real_t *x, const real_t *y, int16_t len, real_t *r){
    for(int16_t i=0; i<len; i++){
        r[i] = x[i] * y[i];
    }
}



real_t vect_std(real_t *x, int16_t len){
    real_t mean = vect_mean(x, len);
    real_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        real_t centered = x[i] - mean;
        RA_IMU_LOG_SCALAR("vect_std", "x_minus_mean", centered);
        real_t sq_dev = centered * centered;
        RA_IMU_LOG_SCALAR("vect_std", "sq_dev", sq_dev);
        sum += sq_dev;
    }

    RA_IMU_LOG_SCALAR("vect_std", "sum_sq_dev", sum);
    real_t variance = sum / len;
    RA_IMU_LOG_SCALAR("vect_std", "variance", variance);
    real_t result = sqrtreal(variance);
    RA_IMU_LOG_SCALAR("vect_std", "result", result);
    return result;
}



void vect_copy(const real_t *in, int16_t start, int16_t len, real_t *out){
    for(int16_t i=0; i<len; i++){
        out[i] = in[i + start];
    }
}




void vect_copy_uint16_t(uint16_t *in, int16_t start, int16_t len, uint16_t *out){
    for(int16_t i=0; i<len; i++){
        out[i] = in[i + start];
    }
}



void sub_constant(const real_t *x, int16_t len, real_t constant, real_t *res){
    for(int16_t i=0; i<len; i++){
        res[i] = x[i] - constant;
    }
}



int16_t vect_max_index(real_t *x, int16_t len){
    int16_t max_i = 0.0;
    real_t temp_v;
    _find_max(x, len, &temp_v, &max_i);
    return max_i;
}



real_t vect_max_value(real_t *x, int16_t len){
    int16_t max_i = 0;
    real_t max_v;
    _find_max(x, len, &max_v, &max_i);
    return max_v;
}



real_t vect_max_abs_value(real_t *x, int16_t len){
    real_t max_abs = x[0];
    real_t tmp = 0.0;
    for(int16_t i=1; i<len; i++){
        tmp = fabs(x[i]);
        if(tmp >= max_abs){
            max_abs = tmp;
        }
    }

    return max_abs;
}




void normalize_max(real_t *x, int16_t len, real_t *res){
    real_t max;
    int16_t max_i;
    _find_max(x, len, &max, &max_i);

    for(int16_t i=0; i<len; i++){
        res[i] = x[i] / max;
    }
}


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
void _find_max(real_t *x, int16_t len, real_t *max_value, int16_t *max_index){
    real_t max_v = x[0];
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



real_t simpson(real_t *x, int16_t len, real_t spacing){
    real_t result = 0.0;

    if(len % 2 == 0){
        real_t val = 0.0;
        val += spacing * (x[len - 1] + x[len - 2]) / 2;
        result += _simpson_step(x, spacing, 0, len-1);

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




/// @brief Helper function that implements the definition of the composite Simpson's integral.
/// This function is not callable externally.
/// @param *x       pointer to the input signal
/// @param spacing  spacing
/// @param start    start index
/// @param end      end index
/// @return
real_t _simpson_step(real_t *x, real_t spacing, int16_t start, int16_t end){

    int n_intervals = (end - start) / 2; // realber of intervals (h in the formula)

    real_t sum = 0.0;
    int interval_start = start;

    // computes the indexes
    for (int i = 0; i < n_intervals; i++)
    {
        sum += x[interval_start] + 4 * x[interval_start+1] + x[interval_start+2];
        interval_start = interval_start + 2;
    }
    return (sum * (spacing / 3));
}


void padding(const real_t *sig, int len, int padlen, real_t *res){

    real_t left_end = sig[0];
    real_t right_end = sig[len-1];

    real_t *left_ext = (real_t*)malloc(padlen * sizeof(real_t));
    real_t *right_ext = (real_t*)malloc(padlen * sizeof(real_t));

    // compute and append padding for the left side
    for(int i=0; i<padlen; i++){
        left_ext[i] = sig[padlen-i];
        right_ext[i] = sig[len-2-i];

        res[i] = (2 * left_end) - left_ext[i];
    }

    // copy the original signal in the central part of the result
    vect_copy(sig, 0, len, &res[padlen]);

    // computes and append padding for the right side
    for(int i=padlen+len; i<(padlen*2)+len; i++){
        res[i] = (2 * right_end) - right_ext[i - (padlen + len)];
    }

    free(left_ext);
    free(right_ext);
}




void zero_padding(const real_t *x, int16_t len, int16_t side_pad_len, real_t *r){

    // pad with zeros in front and in the end
    for(int16_t i=0; i<side_pad_len; i++){
        r[i] = 0.0;
        r[i+side_pad_len+len] = 0.0;
    }

    // copy input vector
    vect_copy(x, 0, len, &r[side_pad_len]);
}



void reflect_padding(const real_t *x, uint16_t len, uint16_t side_pad_len, real_t *r){


    // Adds the padding to head and tail of the array by reversing the original samples
    for(uint16_t i=0; i<side_pad_len; i++){
        r[i] = x[side_pad_len-i];           // head
        r[side_pad_len+len+i] = x[len-2-i]; // tail
    }

    // Copies the original array in the center of the result
    vect_copy(x, 0, len, &r[side_pad_len]);
}




real_t get_line_length(real_t *x, int16_t len){

    real_t sum = 0.0;

    for(int16_t i=0; i<len-1; i++){
        sum += fabs(x[i+1] - x[i]);
        RA_IMU_LOG_SCALAR("get_line_length", "diff", fabs(x[i+1] - x[i]));
    }

    RA_IMU_LOG_SCALAR("get_line_length", "accum", sum);
    real_t result = sum / (len-1);
    RA_IMU_LOG_SCALAR("get_line_length", "result", result);
    return result;
}




real_t get_kurtosis(real_t *x, int16_t len){

    real_t std = vect_std(x, len);
    real_t mean = vect_mean(x, len);

    RA_IMU_LOG_SCALAR("get_kurtosis", "mean", mean);
    RA_IMU_LOG_SCALAR("get_kurtosis", "std", std);

    real_t sum = 0.0;
#ifdef RANGE_ANALYSIS
    real_t moment_max = 0.0;
#endif

    for(int16_t i=0; i<len; i++){
        real_t tmp = (x[i] - mean) * (x[i] - mean);
        real_t x4 = tmp * tmp;
#ifdef RANGE_ANALYSIS
        if(x4 > moment_max)
            moment_max = x4;
#endif
        sum += x4;
    }

#ifdef RANGE_ANALYSIS
    RA_IMU_LOG_SCALAR("get_kurtosis", "moment_max", moment_max);
#endif
    RA_IMU_LOG_SCALAR("get_kurtosis", "sum_x4", sum);

    real_t std4 = pow(std, 4);
    RA_IMU_LOG_SCALAR("get_kurtosis", "std4", std4);

    real_t result = (sum / (len * std4)) - KURT_FISHER_CONST;
    RA_IMU_LOG_SCALAR("get_kurtosis", "result", result);
    return result;
}


real_t L2_norm(const real_t *x, int16_t len){

    real_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        sum += x[i] * x[i];
    }

    RA_IMU_LOG_SCALAR("L2_norm", "sum_sq", sum);
    real_t result = sqrtreal(sum);
    RA_IMU_LOG_SCALAR("L2_norm", "result", result);
    return result;
}



uint16_t min(uint16_t a, uint16_t b){
    if(a < b)
        return a;
    else
        return b;
}



void entropy_calc(real_t *x, int16_t len, uint8_t base){

    for(int16_t i=0; i<len; i++){
        if(x[i] > 0.0){
            x[i] = -1.0 * x[i] * logreal(x[i]);
        }
        else if(x[i] < 0.0){
            x[i] = MIN_FLOAT;
        }
    }

    // If base needed, change the base of the logarithm result
    if(base != 1){
        for(int16_t i=0; i<len; i++){
            if(x[i] > MIN_FLOAT){
                x[i] /= logreal(base);
            }
        }
    }
}
