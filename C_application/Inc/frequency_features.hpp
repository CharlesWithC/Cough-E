#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <constants.h>
#include <types.h>

#include <audio_features.h>
#include <feature_extraction.h>
#include <helpers.h>
#include <kiss_fftr.h>
#include <mfcc_module.h>
#include <range_analysis.h>

#ifndef FXP_MODE

/*
    Helper function not callable externally.
    Stores re/im as T (original behaviour).
*/
template <RealType T> void _rfft(const T *sig, int16_t len, T *real, T *imag);

template <RealType T, RealType S = T> void compute_rfft(const T *sig, int16_t len, int16_t fs, T *mags, T *freqs, T *sum_mags) {
    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "sig_input", sig, len);

    T *re = (T *)malloc(len * sizeof(T));
    T *im = (T *)malloc(len * sizeof(T));
    _rfft(sig, len, re, im);

    int16_t fft_size = (len / 2) + 1;
    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "re", re, fft_size);
    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "im", im, fft_size);

    // Compute the magnitude of each FFT output
    S sum_mags_local = *sum_mags; // use high precision accumulator to reduce rounding error
    for (int16_t i = 0; i < fft_size; i++) {
        mags[i] = sqrtreal((re[i] * re[i]) + (im[i] * im[i]));
        sum_mags_local += mags[i];
    }
    *sum_mags = sum_mags_local;

    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "magnitudes", mags, fft_size);

    // Get the frequency bins (only the positive one becaus it's a real FFT)
    for (int16_t i = 0; i < fft_size; i++) {
        freqs[i] = (T)(i * fs) / len;
    }

    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "frequencies", freqs, fft_size);
    RA_LOG_SCALAR("AUDIO_FFT", "compute_rfft", "sum_mags", *sum_mags);

    free(re);
    free(im);
}

template <RealType T> void _rfft(const T *sig, int16_t len, T *real, T *imag) {
    kiss_fftr_cfg<T> cfg = kiss_fftr_alloc<T>(len, 0, 0, 0);
    int16_t fft_size = (len / 2) + 1;
    kiss_fft_cpx<T> *cx_out = (kiss_fft_cpx<T> *)malloc((size_t)fft_size * sizeof(kiss_fft_cpx<T>));

    kiss_fftr(cfg, sig, cx_out);
    for (int16_t i = 0; i < fft_size; i++) {
        real[i] = cx_out[i].r;
        imag[i] = cx_out[i].i;
    }

    free(cfg);
    free(cx_out);
}

template <RealType T, RealType S = T>
void compute_periodogram(const T *sig, int16_t len, int16_t fs, T *psd, T *freqs) {
    T freq_step = ((T)fs / 2) / ((T)NPERSEG / 2);     // the frequency step for eah bin
    T *win = (T *)malloc(NPERSEG * sizeof(T));        // to keep the data of the current processed window
    S *cumul_sums = (S *)malloc(NPERSEG * sizeof(S)); // To store the cumulative sum of the FFT of each frequency bin
    for (int i = 0; i < NPERSEG; i++)
        cumul_sums[i] = 0;

    T *re = (T *)malloc(NPERSEG * sizeof(T));
    T *im = (T *)malloc(NPERSEG * sizeof(T));

    // To store the magnitudes squared after the FFT
    T *mags_squared = (T *)malloc(((NPERSEG / 2) + 1) * sizeof(T));

    T mean = 0.0;
    S scale = 0.0;
    S sum = 0.0;

    for (int16_t i = 0; i < NPERSEG; i++) {
        sum += hann_window<T>[i] * hann_window<T>[i];
    }

    scale = 1 / (fs * sum);
    RA_LOG_SCALAR("AUDIO_PSD", "periodogram", "scale", scale);

    // start and end indexes of the current processed window
    int16_t start = 0;
    int16_t end = start + NPERSEG;

    int16_t psd_size = (NPERSEG / 2) + 1;
    int16_t steps = (len - NOVERLAP) / (NPERSEG - NOVERLAP); // Number or windows that will be processed
    for (int16_t i = 0; i < steps; i++) {

        vect_copy(sig, start, NPERSEG, win); // copies the current window from the signal

        // subtract the mean
        mean = vect_mean<T, S>(win, NPERSEG);
        sub_constant(win, NPERSEG, mean, win);

        // Apply the window function
        for (int16_t i = 0; i < NPERSEG; i++) {
            win[i] *= hann_window<T>[i];
        }

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "windowed", win, NPERSEG);

        _rfft(win, NPERSEG, re, im); // Actual Real FFT computation

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "re", re, psd_size);
        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "im", im, psd_size);

        for (int16_t i = 0; i < psd_size; i++) {
            mags_squared[i] = (S)((re[i] * re[i]) + (im[i] * im[i])) * scale;

            if (i != 0 && i != (NPERSEG / 2)) {
                mags_squared[i] *= 2; // Multiply by 2, apart from DC frequency (first element) and last element
            }
            cumul_sums[i] +=
                mags_squared[i]; // Update the cumulative sum (element-wise across FFT result of different windows)
        }

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "mags_squared", mags_squared, psd_size);

        start = end - NOVERLAP;
        end = start + NPERSEG;
    }

    RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "cumul_sums", cumul_sums, psd_size);

    // Compute the result psd as the avarage of each FFT result and compute the frequencies
    for (int16_t i = 0; i < psd_size; i++) {
        psd[i] = cumul_sums[i] / steps;
        freqs[i] = freq_step * i;
    }

    RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "psd", psd, psd_size);
    RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "freqs", freqs, psd_size);

    free(win);
    free(cumul_sums);
    free(re);
    free(im);
    free(mags_squared);
}

template <RealType T> T compute_spec_decrease(T *mags, T *freqs, int16_t len, T sum_mags) {
    T sum = 0.0;
    T dc_mag = mags[0];

    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "dc_mag", dc_mag);

    for (int16_t i = 0; i < len; i++) {
        sum += (mags[i] - dc_mag) / (i + 1);
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "sum", sum);
    T result = sum / sum_mags;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "result", result);
    return result;
}

template <RealType T> T compute_spectral_slope(T *mags, T *freqs, int16_t len, T sum_mags) {
    T mean_mag = sum_mags / len;
    T mean_freq = 0.0;

    mean_freq = vect_mean(freqs, len);

    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "mean_mag", mean_mag);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "mean_freq", mean_freq);

    // Numerator and denominator for the final slope computation
    T num = 0.0;
    T den = 0.0;

    for (int16_t i = 0; i < len; i++) {
        num += (freqs[i] - mean_freq) * (mags[i] - mean_mag);
        den += (freqs[i] - mean_freq) * (freqs[i] - mean_freq);
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "num", num);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "den", den);
    T result = num / den;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "result", result);
    return result;
}

template <RealType T, RealType S = T> T compute_rolloff(T *mags, T *freqs, int16_t len, T sum_mags) {
    S rolloff_energy = 0.95 * (S)sum_mags;
    S sum = 0.0;
    T rolloff = -1.0; // Error value

    RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "rolloff_energy", rolloff_energy);

    for (int16_t i = 0; i < len; i++) {
        sum += mags[i];
        RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "sum", sum);

        if (sum >= rolloff_energy) {
            RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "result", freqs[i]);
            return freqs[i];
        }
    }

    RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "result", rolloff);
    return rolloff;
}

template <RealType T> T compute_centroid(T *mags, T *freqs, int16_t len, T sum_mags) {
    T sum = 0.0;

    for (int16_t i = 0; i < len; i++) {
        sum += freqs[i] * mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "centroid", "sum", sum);
    T result = sum / sum_mags;
    RA_LOG_SCALAR("AUDIO_FFT", "centroid", "result", result);
    return result;
}

// allow higher precision selection (based on precision analysis result)
template <RealType T, RealType S = T> T compute_spread(T *mags, T *freqs, int16_t len, T sum_mags, T centroid) {
    S sum = 0.0;

    for (int16_t i = 0; i < len; i++) {
        sum += (freqs[i] - centroid) * (freqs[i] - centroid) * mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spread", "sum", sum);
    T result = sqrtreal(sum / (S)sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spread", "result", result);
    return result;
}

// allow higher precision selection (based on precision analysis result)
template <RealType T, RealType S = T> T compute_kurt(T *mags, T *freqs, int16_t len, T sum_mags, T centroid, T spread) {
    S spread_4 = spread * spread * spread * spread; // spread^4
    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "spread_4", spread_4);

    S sum = 0.0;

    for (int16_t i = 0; i < len; i++) {
        T tmp = (freqs[i] - centroid) * (freqs[i] - centroid);
        sum += tmp * tmp * mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "sum", sum);
    T result = sum / (spread_4 * (S)sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "result", result);
    return result;
}

template <RealType T> T compute_skew(T *mags, T *freqs, int16_t len, T sum_mags, T centroid, T spread) {
    T spread_3 = spread * spread * spread;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "spread_3", spread_3);

    T sum = 0.0;

    for (int16_t i = 0; i < len; i++) {
        T tmp = (freqs[i] - centroid) * (freqs[i] - centroid);
        sum += tmp * (freqs[i] - centroid) * mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "sum", sum);
    T result = sum / (spread_3 * sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "result", result);
    return result;
}

// allow higher precision selection (based on precision analysis result)
template <RealType T, RealType S = T> T compute_flatness(T *x, int16_t len) {
    RA_LOG_ARRAY("AUDIO_PSD", "flatness", "input", x, len);

    T gmean = 0.0; // geometric
    T amean = 0.0; // arithmetic

    S sum_logs = 0.0;
    for (int16_t i = 0; i < len; i++) {
        T log_val = logreal(x[i]);
        RA_LOG_SCALAR("AUDIO_PSD", "flatness", "log_val", log_val);
        sum_logs += log_val;
    }
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "sum_logs_raw", sum_logs);
    sum_logs = sum_logs / len;

    gmean = expreal(sum_logs);
    amean = vect_mean<T, S>(x, len);

    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "sum_logs", sum_logs);
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "gmean", gmean);
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "amean", amean);
    T result = gmean / amean;
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "result", result);
    return result;
}

template <RealType T> T compute_std(T *x, int16_t len) {
    RA_LOG_ARRAY("AUDIO_PSD", "spec_std", "input", x, len);
    T result = vect_std(x, len);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_std", "result", result);
    return result;
}

template <RealType T> T compute_spectral_entropy(T *x, int16_t len) {
    RA_LOG_ARRAY("AUDIO_PSD", "spec_entropy", "input", x, len);

    T *tmp = (T *)malloc(len * sizeof(T));
    T sum = vect_sum(x, len);
    vect_div_const(x, len, sum, tmp);
    entropy_calc(tmp, len, 2);

    T result = vect_sum(tmp, len);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_entropy", "sum", sum);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_entropy", "result", result);

    free(tmp);

    return result;
}

template <RealType T> T get_domiant_freq(T *psd, T *freqs, int16_t len) {
    T result = freqs[vect_max_index(psd, len)];
    RA_LOG_SCALAR("AUDIO_PSD", "dominant_freq", "result", result);
    return result;
}

template <RealType T>
void normalized_bandpowers(T *psd, T *freqs, int16_t len, const int8_t *psd_selector, T *band_powers) {
    RA_LOG_ARRAY("AUDIO_PSD", "bandpowers", "psd_input", psd, len);

    T dx_freq = freqs[1] - freqs[0];
    T total_power = simpson(psd, len, dx_freq);

    RA_LOG_SCALAR("AUDIO_PSD", "bandpowers", "total_power", total_power);

    int16_t start_ind_freq = 0; // index of the first frequency inside the band
    int16_t n_bins = 0;         // number of frequency bins inside the band
    int8_t start_found = 0;     // 1 if the start frequency was found, useful to minimize the if-statements

    T band_power = 0.0;

    // check which PSD bands are needed
    for (int16_t i = 0; i < N_PSD; i++) {
        if (psd_selector[i] == 1) {
            start_found = 0;
            start_ind_freq = 0;
            n_bins = 0;
            for (int16_t j = 0; j < len; j++) {
                if (!start_found && freqs[j] >= psd_bands[i].start) {
                    start_ind_freq = j;
                    start_found = 1;
                }
                if (start_found && freqs[j] <= psd_bands[i].end) {
                    n_bins++;
                }
            }

            band_power = simpson(&psd[start_ind_freq], n_bins, dx_freq);
            RA_LOG_SCALAR("AUDIO_PSD", "bandpowers", "band_power", band_power);
            band_powers[i] = band_power / total_power;
            RA_LOG_SCALAR("AUDIO_PSD", "bandpowers", "result", band_powers[i]);
        }
    }
}

template <RealType T> void mfcc_computation(const T *x, int16_t len, int16_t n_frames, T *coeffs) {
    T *db_power = (T *)malloc((MEL_ROWS * n_frames) * sizeof(T));
    for (int i = 0; i < MEL_ROWS * n_frames; i++)
        db_power[i] = 0;

    // Mel spectrogram
    mel_spectrogram_full(x, len, n_frames, db_power);

    // Convert the power in dB
    power_to_dB(db_power, (MEL_ROWS * n_frames), db_power);

    // Apply the DCT
    dct_matrix(db_power, MEL_ROWS, n_frames, db_power);

    for (int16_t i = 0; i < N_MFCC; i++) {
        for (int16_t j = 0; j < n_frames; j++) {
            coeffs[(i * n_frames) + j] = db_power[(i * n_frames) + j];
        }
    }

    free(db_power);
}

template <RealType T> void get_mfcc_features(const T *x, int16_t len, T *mean_mfcc, T *std_mfcc) {
    int16_t padded_len = (2 * PAD_LEN) + len;                // lenght of the padded signal
    int16_t n_frames = ((padded_len - N_FFT) / HOP_LEN) + 1; // number of frames for the stft

    T *coeffs = (T *)malloc((N_MFCC * n_frames) * sizeof(T));
    mfcc_computation(x, len, n_frames, coeffs);

    for (int16_t i = 0; i < N_MFCC; i++) {
        mean_mfcc[i] = vect_mean(&coeffs[i * n_frames], n_frames);
        std_mfcc[i] = vect_std(&coeffs[i * n_frames], n_frames);
    }

    free(coeffs);
}

// allow higher precision selection (based on precision analysis result)
template <RealType T, RealType S = T>
void get_mel_spectrogram_features(const T *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed,
                                  S *mean_mel_spectr, S *std_mel_spectr, S *max_mel_spectr, S *entropy_mel_spectr) {
    int16_t padded_len = (2 * PAD_LEN) + len;                // lenght of the padded signal
    int16_t n_frames = ((padded_len - N_FFT) / HOP_LEN) + 1; // number of frames for the stft

    T *spectrogram = (T *)malloc((n_mels_needed * n_frames) * sizeof(T));
    for (int i = 0; i < n_mels_needed * n_frames; i++)
        spectrogram[i] = 0;

    // Get the spectrogram
    mel_spectrogram(x, len, n_frames, idx_needed, spectrogram);

    // Stores the dB of the mel spectrogram
    T *mel_dB = (T *)malloc((n_mels_needed * n_frames) * sizeof(T));

    // Convert the power of the spectrogram in dB
    power_to_dB(spectrogram, (n_mels_needed * n_frames), mel_dB);

    // Computes the entropy of the spectrogram
    entropy(spectrogram, n_mels_needed, n_frames, entropy_mel_spectr);

    // Computes the mean, std and maximum value of each MEL bin
    for (int8_t i = 0; i < n_mels_needed; i++) {
        mean_mel_spectr[i] = vect_mean<T, S>(&mel_dB[i * n_frames], n_frames);
        std_mel_spectr[i] = vect_std<T, S>(&mel_dB[i * n_frames], n_frames);
        max_mel_spectr[i] = vect_max_value(&mel_dB[i * n_frames], n_frames);
    }

    RA_LOG_ARRAY("AUDIO_MEL", "mel_features", "mel_dB", mel_dB, n_mels_needed * n_frames);
    RA_LOG_ARRAY("AUDIO_MEL", "mel_features", "mean", mean_mel_spectr, n_mels_needed);
    RA_LOG_ARRAY("AUDIO_MEL", "mel_features", "std", std_mel_spectr, n_mels_needed);
    RA_LOG_ARRAY("AUDIO_MEL", "mel_features", "max", max_mel_spectr, n_mels_needed);
    RA_LOG_ARRAY("AUDIO_MEL", "mel_features", "entropy", entropy_mel_spectr, n_mels_needed);

    free(spectrogram);
    free(mel_dB);
}

#endif /* !FXP_MODE */
