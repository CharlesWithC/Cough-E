#ifndef _TIME_DOMAIN_FEAT_H
#define _TIME_DOMAIN_FEAT_H

#include <inttypes.h>
#include <types.h>
#include <helpers.h>



/// @brief Returns the max value of an array
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return         the maximum value
template <typename T>
static inline T get_max(T *sig, int16_t len){

    T max = sig[0];

    for(int16_t i=1; i<len; i++){
        if(sig[i] > max){
            max = sig[i];
        }
    }
    //RA_IMU_LOG_SCALAR("get_max", "get_max", max);
    return max;
}



/// @brief Subtracts to each sample of a signal its mean
/// @param *sig     pointer to the input array
/// @param *res     pointer to the resulting array
/// @param len      lenght of the input array
void sub_mean(const real_t *sig, real_t *res, int16_t len);



/// @brief Returns the RMS of the input signal
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return The root mean squared
template <typename T>
static inline T get_rms(T *sig, int16_t len){
    T sum = 0;
    for(int16_t i=0; i<len; i++){
        T sq = sig[i] * sig[i];
        //RA_IMU_LOG_SCALAR("get_rms", "sig_sq", sq);
        sum += sq;
    }
    //RA_IMU_LOG_SCALAR("get_rms", "sum_sq", sum);
    T result = sqrtreal(sum / len);
    //RA_IMU_LOG_SCALAR("get_rms", "result", result);
    return result;
}


/// @brief Computes the Zero Crossing Rate
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return the zero crossing rate of the signal
template <typename T>
static inline T compute_zrc(T *sig, int16_t len){

    int sum = 0;
    T interm_product = 0;

    for(int16_t i=0; i<len-1; i++){
        interm_product = sig[i] * sig[i + 1];
        //RA_IMU_LOG_SCALAR("compute_zrc", "multiplier", interm_product);
        if(interm_product < 0){
            sum++;
        }
    }
    T result = (T) sum / (len - 1);
    //RA_IMU_LOG_SCALAR("compute_zrc", "result", result);
    return result;
}




/// @brief Computes the EEPD features
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @param fs       the sampling frequency
/// @param *select  pointer to the selector specifying the required eepd values
/// @param *res     pointer to the resulting array
void eepd(const real_t *sig, int16_t len, int16_t fs, const int8_t *select, int16_t *res);

#endif
