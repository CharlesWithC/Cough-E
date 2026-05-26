#ifndef _FREQ_FEAT_H_
#define _FREQ_FEAT_H_

#include <inttypes.h>
#include <types.h>

#include <welch_psd.h>

/*
    Set of functions to compute the RFFT (Real FFT) and spectral features of a signal
*/


/// @brief Computes the Real FFT of a time signal sig.
/// It stores the magnitudes, the frequencies and the sum of all the magnitudes inside
/// *mags, *freqs abd *sum_mags, respectively.
/// Note that also the sampling_frequency (fs) and the length (len) of the signal are required as inputs
///
/// @param *sig         pointer to the signal
/// @param len          length of the signal
/// @param fs           sampling frequency
/// @param *mags        pointer of the array in which to store the magnitudes of the result
/// @param *freqs       pointer of the array in which to store the frequencies of the result
/// @param *sum_mags    pointer to the value that stores the sum of the resulting magnitudes
void compute_rfft(const num_t *sig, int16_t len, int16_t fs, num_t *mags, num_t *freqs, num_t *sum_mags);



/// @brief Computes the periodogram of a signal using the Welch's method
/// @param *sig     pointer to the signal
/// @param len      length of the signal
/// @param fs       sampling frequency
/// @param *psd     pointer to the array that stores the resulting power spectral densities
/// @param *freqs   pointer to the array that stores the resulting frequencies
void compute_periodogram(const num_t *sig, int16_t len, int16_t fs, num_t *psd, num_t *freqs);



/// @brief Returns the spectral decrease computed from the magnitudes and the frequencies of the spectrum of a signal
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @return spectral decrease
num_t compute_spec_decrease(num_t* mags, num_t* freqs, int16_t len, num_t sum_mags);



/// @brief Returns the spectral slope computed from the magnitudes and the frequencies of the spectrum of a signal
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @return spectral slope
num_t compute_spectral_slope(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags);



/// @brief Returns the spectral roll off computed from the magnitudes and the frequencies of the spectrum of a signal
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @return rolloff
num_t compute_rolloff(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags);



/// @brief Returns the spectral centroid computed from the magnitudes and the frequencies of the spectrum of a signal
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @return centroid
num_t compute_centroid(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags);



/// @brief Returns the spectral spread computed from the magnitudes and the frequencies of
/// the spectrum of a signal. Note that this also requires the spectral centroid as an input
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @param centroid     centroid
/// @return spread
num_t compute_spread(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags, num_t centroid);



/// @brief Returns the spectral kurtosis computed from the magnitudes and the frequencies of
/// the spectrum of a signal. Note that this also requires the spectral centroid and the spectral spread as inputs
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @param centroid     centroid
/// @param spread       spread
/// @return kurtosis
num_t compute_kurt(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags, num_t centroid, num_t spread);



/// @brief Returns the spectral skewness computed from the magnitudes and the frequencies of
/// the spectrum of a signal. Note that this also requires the spectral centroid and the spectral spread as inputs
/// @param *mags        pointer to the magnitudes
/// @param *freqs       pointer to the frequencies
/// @param len          length
/// @param sum_mags     sum of the magnitues
/// @param centroid     centroid
/// @param spread       spread
/// @return skewness
num_t compute_skew(num_t *mags, num_t *freqs, int16_t len, num_t sum_mags, num_t centroid, num_t spread);



/// @brief Returns the spectral flatness of the given signal
/// @param *x   pointer to the signal
/// @param len  lenght of the signal
/// @return     the flatness of the signal
num_t compute_flatness(num_t *x, int16_t len);



/// @brief Returns the standard deviation of the given signal
/// @param *x   pointer to the signal
/// @param len  lenght of the signal
/// @return     the standard deviation of the signal
num_t compute_std(num_t *x, int16_t len);



/// @brief Returns the spectral entropy of the given signal
/// @param *x   pointer to the signal
/// @param len  lenght of the signal
/// @return     the spectral entropy of the signal
num_t compute_spectral_entropy(num_t *x, int16_t len);



/// @brief  Returns the frequency at which the maximum psd is found
/// @param *psd     pointer power spectral density of the signal
/// @param *freqs   pointer to the frequencies
/// @param len      lenght
/// @return         the dominant frequency
num_t get_domiant_freq(num_t *psd, num_t *freqs, int16_t len);



/// @brief Computes the normalized power of each band.
/// It requires the psd, the frequencies and also the psd_selector, which is an array
/// of 1 or 0. 1 indicates that a specific band has to be computed. The bands
/// are defined by the "psd_bands" structure
/// @param *psd             pointer to the power spectral density of the signal
/// @param *freqs           pointer to the frequencies
/// @param len              lenght
/// @param *psd_selector    pointer to the selector for the psd
/// @param *band_powers     poitner to the resulting band powers
void normalized_bandpowers(num_t *psd, num_t *freqs, int16_t len, const int8_t *psd_selector, num_t *band_powers);



/// @brief Computes the MFCC coefficients of the given signal x
/// This function uses the STFT technique, therefore the MFCC are computed
/// for every frame in which the RFFT is computed.
/// Basically the output will be a matrix having, on i-th row, the i-th coefficient
/// for each signal frame.
/// The matrix is stored in a 1-Dimentional array, storing each row one after the other:
/// coeffs = [... ROW 0 ... | ... ROW 1 ... | ...]
/// @param *x       pointer to the signal
/// @param len      lenght of the signal
/// @param n_frames number of frames
/// @param *coeffs  pointer to the resulting coefficients
void mfcc_computation(const num_t *x, int16_t len, int16_t n_frames, num_t *coeffs);



/// @brief Computes the MFCC reletad features
/// @param *x           pointer to the signal
/// @param len          lenght of the signa;
/// @param *mean_mfcc   pointer to the resulting mean of the MFCC
/// @param *std_mfcc    pointer to the resulting standard deviation of the MFCC
void get_mfcc_features(const num_t *x, int16_t len, num_t *mean_mfcc, num_t *std_mfcc);



/// @brief Computes the features related to the mel spectrogram
/// @param *x                   pointer to the signal
/// @param len                  lenght of the signal
/// @param *idx_needed          pointr to the array specifying the needed indexes for the mel spectrogram
/// @param n_mels_needed        number of needed mels
/// @param *mean_mel_spectr     pointer to the resulting mean of the mels
/// @param *std_mel_spectr      pointer to the resulting standard deviations of the mels
/// @param *max_mel_spectr      pointer to the resulting max of the mels
/// @param *entropy_mel_spectr  pointer to the resulting entropies of the mels
void get_mel_spectrogram_features(const num_t *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed, num_t *mean_mel_spectr, num_t *std_mel_spectr, num_t *max_mel_spectr, num_t *entropy_mel_spectr);

#endif
