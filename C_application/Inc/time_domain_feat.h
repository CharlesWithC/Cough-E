#ifndef _TIME_DOMAIN_FEAT_H
#define _TIME_DOMAIN_FEAT_H

#include <math.h>
#include <stdlib.h>
#include <types.h>

#include <feature_extraction.h>
#include <filtering.h>
#include <filters_parameters.h>
#include <helpers.h>

#include <audio_features.h>
#include <imu_features.h>
#include <range_analysis.h>

// Here I put all the functions to compute time domain features

/*
    Helper function to count the number of local maxima in a signal.
    This function mimics the _local_maxima_1d() function in the python
    "scipy" module.
*/
template <RealType T> static inline int16_t _find_peaks(T *x, int16_t len) {
    int16_t i = 1; // points to the first considered sample (the second one)
    int16_t i_max = len - 1;
    int16_t i_ahead = 0;

    int16_t npeaks = 0;

    while (i < i_max) {
        if (x[i - 1] < x[i]) {
            i_ahead = i + 1;

            while (i_ahead < i_max && x[i_ahead] == x[i]) {
                i_ahead++;
            }

            if (x[i_ahead] < x[i]) {
                npeaks++;
                i = i_ahead;
            }
        }
        i++;
    }
    return npeaks;
}

/// @brief Returns the max value of an array
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return         the maximum value
template <RealType T> static inline T get_max(T *sig, int16_t len) {
    T max = sig[0];

    for (int16_t i = 1; i < len; i++) {
        if (sig[i] > max) {
            max = sig[i];
        }
    }

    RA_IMU_LOG_SCALAR("get_max", "get_max", max);
    return max;
}

/// @brief Subtracts to each sample of a signal its mean
/// @param *sig     pointer to the input array
/// @param *res     pointer to the resulting array
/// @param len      lenght of the input array
template <RealType T>
static inline void sub_mean(const T *sig, T *res, int16_t len) {
    T mean = vect_mean(sig, len);

    sub_constant(sig, len, mean, res);
}

/// @brief Returns the RMS of the input signal
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return The root mean squared
template <RealType T> static inline T get_rms(T *sig, int16_t len) {
    T sum = 0;
    for (int16_t i = 0; i < len; i++) {
        T sq = sig[i] * sig[i];
        RA_IMU_LOG_SCALAR("get_rms", "sig_sq", sq);
        sum += sq;
    }
    RA_IMU_LOG_SCALAR("get_rms", "sum_sq", sum);
    T result = sqrtreal(sum / len);
    RA_IMU_LOG_SCALAR("get_rms", "result", result);
    return result;
}

/// @brief Computes the Zero Crossing Rate
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return the zero crossing rate of the signal
template <RealType T> static inline T compute_zrc(T *sig, int16_t len) {
    int sum = 0;
    T interm_product = 0;

    for (int16_t i = 0; i < len - 1; i++) {
        interm_product = sig[i] * sig[i + 1];
        RA_IMU_LOG_SCALAR("compute_zrc", "multiplier", interm_product);
        if (interm_product < 0) {
            sum++;
        }
    }
    T result = (T)sum / (len - 1);
    RA_IMU_LOG_SCALAR("compute_zrc", "result", result);
    return result;
}

/// @brief Computes the EEPD features
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @param fs       the sampling frequency
/// @param *select  pointer to the selector specifying the required eepd values
/// @param *res     pointer to the resulting array
template <RealType T>
static inline void eepd(const T *sig, int16_t len, int16_t fs,
                        const int8_t *select, int16_t *res) {
    T *interm =
        (T *)malloc(len * sizeof(T)); // to store the intermediate result
                                      // between the first and the second filter
    T *filtered = (T *)malloc(
        len * sizeof(T)); // temporary to store the result of each filter

    const T *b, *a, *zi;

    for (int16_t i = 0; i < N_EEPD; i++) {
        if (select[i] == 1) {

            b = filters_parameters.filters[i].b;
            a = filters_parameters.filters[i].a;
            zi = filters_parameters.filters[i].zi;

            filtfilt(sig, len, b, a, zi, interm);
            RA_LOG_ARRAY("AUDIO_EEPD", "eepd", "bandpass_out", interm, len);

            vect_mult(interm, interm, len, interm); // squared vector
            RA_LOG_ARRAY("AUDIO_EEPD", "eepd", "squared", interm, len);

            filtfilt(interm, len, b_second, a_second, zi_second, filtered);
            RA_LOG_ARRAY("AUDIO_EEPD", "eepd", "envelope", filtered, len);

            normalize_max(filtered, len,
                          filtered); // divide each number by the maximum
            RA_LOG_ARRAY("AUDIO_EEPD", "eepd", "normalized", filtered, len);

            res[i] = _find_peaks(filtered, len);
            RA_LOG_SCALAR("AUDIO_EEPD", "eepd", "n_peaks", (T)res[i]);
        }
    }

    free(interm);
    free(filtered);
}

#endif
