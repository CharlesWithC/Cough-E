#ifndef _TIME_DOMAIN_FEAT_H
#define _TIME_DOMAIN_FEAT_H

#include <inttypes.h>
#include <types.h>



/// @brief Returns the max value of an array
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return         the maximum value
real_t get_max(real_t *sig, int16_t len);



/// @brief Subtracts to each sample of a signal its mean
/// @param *sig     pointer to the input array
/// @param *res     pointer to the resulting array
/// @param len      lenght of the input array
void sub_mean(const real_t *sig, real_t *res, int16_t len);



/// @brief Returns the RMS of the input signal
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return The root mean squared
real_t get_rms(real_t *sig, int16_t len);


/// @brief Computes the Zero Crossing Rate
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @return the zero crossing rate of the signal
real_t compute_zrc(real_t *sig, int16_t len);




/// @brief Computes the EEPD features
/// @param *sig     pointer to the input signal
/// @param len      lenght of the input signal
/// @param fs       the sampling frequency
/// @param *select  pointer to the selector specifying the required eepd values
/// @param *res     pointer to the resulting array
void eepd(const real_t *sig, int16_t len, int16_t fs, const int8_t *select, int16_t *res);

#endif
