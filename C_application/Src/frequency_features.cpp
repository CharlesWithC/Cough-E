#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <types.h>
#include <strings.h>

#include <feature_extraction.h>
#include <frequency_features.h>
#include <welch_psd.h>
#include <mfcc_module.h>
#include <mel_basis.h>
#include <helpers.h>

#include <audio_features.h>

#include <kiss_fftr.h>
#include <range_analysis.h>

#ifndef FXP_MODE

/*
    Helper function not callable externally.
    Stores re/im as audio_sample_t (original behaviour).
*/
void _rfft(const audio_sample_t *sig, int16_t len, audio_sample_t *real, audio_sample_t *imag);


void compute_rfft(const audio_sample_t *sig, int16_t len, int16_t fs, audio_sample_t *mags, audio_sample_t *freqs, audio_feat_t *sum_mags){

    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "sig_input", sig, len);

    audio_sample_t *re = (audio_sample_t*)malloc(len * sizeof(audio_sample_t));
    audio_sample_t *im = (audio_sample_t*)malloc(len * sizeof(audio_sample_t));
    _rfft(sig, len, re, im);

    int16_t fft_size = (len/2)+1;
    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "re", re, fft_size);
    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "im", im, fft_size);

    // Compute the magnitude of each FFT output
    for(int16_t i=0; i<fft_size; i++){
        mags[i] = sqrtreal((re[i] * re[i]) + (im[i] * im[i]));
        *sum_mags += mags[i];
    }

    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "magnitudes", mags, fft_size);

    // Get the frequency bins (only the positive one becaus it's a real FFT)
    for(int16_t i=0; i<fft_size; i++){
        freqs[i] = (audio_sample_t)(i * fs) / len;
    }

    RA_LOG_ARRAY("AUDIO_FFT", "compute_rfft", "frequencies", freqs, fft_size);
    RA_LOG_SCALAR("AUDIO_FFT", "compute_rfft", "sum_mags", *sum_mags);

    free(re);
    free(im);
}



void _rfft(const audio_sample_t *sig, int16_t len, audio_sample_t *real, audio_sample_t *imag){

    kiss_fftr_cfg cfg = kiss_fftr_alloc(len, 0, 0, 0);
    int16_t fft_size = (len / 2) + 1;
    kiss_fft_cpx *cx_out = (kiss_fft_cpx *) malloc((size_t)fft_size * sizeof(kiss_fft_cpx));

    kiss_fftr(cfg, sig, cx_out);
    for(int16_t i=0; i<fft_size; i++){
        real[i] = cx_out[i].r;
        imag[i] = cx_out[i].i;
    }

    free(cfg);
    free(cx_out);
}



void compute_periodogram(const audio_sample_t *sig, int16_t len, int16_t fs, audio_sample_t *psd, audio_sample_t *freqs){

    audio_sample_t freq_step = ((audio_sample_t)fs / 2) / ( (audio_sample_t)NPERSEG / 2);  // the frequency step for eah bin
    audio_sample_t *win = (audio_sample_t*)malloc(NPERSEG * sizeof(audio_sample_t));   // to keep the data of the current processed window
    audio_sample_t *cumul_sums = (audio_sample_t*)malloc(NPERSEG * sizeof(audio_sample_t));  // To store the cumulative sum of the FFT of each frequency bin
    for (int i = 0; i < NPERSEG; i++) cumul_sums[i] = 0;

    audio_sample_t *re = (audio_sample_t*)malloc(NPERSEG * sizeof(audio_sample_t));
    audio_sample_t *im = (audio_sample_t*)malloc(NPERSEG * sizeof(audio_sample_t));

    // To store the magnitudes squared after the FFT
    audio_sample_t *mags_squared = (audio_sample_t*)malloc(((NPERSEG/2)+1) * sizeof(audio_sample_t));


    audio_sample_t mean = 0.0;
    audio_sample_t scale = 0.0;
    audio_sample_t sum = 0.0;

    for(int16_t i=0; i<NPERSEG; i++){
        sum += hann_window[i] * hann_window[i];
    }

    scale = 1 / (fs * sum);
    RA_LOG_SCALAR("AUDIO_PSD", "periodogram", "scale", scale);

    // start and end indexes of the current processed window
    int16_t start = 0;
    int16_t end = start + NPERSEG;

    int16_t psd_size = (NPERSEG/2)+1;
    int16_t steps = (len - NOVERLAP) / (NPERSEG - NOVERLAP);    // Number or windows that will be processed
    for(int16_t i=0; i<steps; i++){

        vect_copy(sig, start, NPERSEG, win);    // copies the current window from the signal

        // subtract the mean
        mean = vect_mean<audio_sample_t>(win, NPERSEG);
        sub_constant(win, NPERSEG, mean, win);

        // Apply the window function
        for(int16_t i=0; i<NPERSEG; i++){
            win[i] *= hann_window[i];
        }

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "windowed", win, NPERSEG);

        _rfft(win, NPERSEG, re, im);    // Actual Real FFT computation

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "re", re, psd_size);
        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "im", im, psd_size);

        for(int16_t i=0; i<psd_size; i++){
            mags_squared[i] = ((re[i] * re[i]) + (im[i] * im[i])) * scale;

            if(i != 0 && i != (NPERSEG/2)){
                mags_squared[i] *= 2;       // Multiply by 2, apart from DC frequency (first element) and last element
            }
            cumul_sums[i] += mags_squared[i];   // Update the cumulative sum (element-wise across FFT result of different windows)
        }

        RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "mags_squared", mags_squared, psd_size);

        start = end - NOVERLAP;
        end = start + NPERSEG;
    }

    RA_LOG_ARRAY("AUDIO_PSD", "periodogram", "cumul_sums", cumul_sums, psd_size);

    // Compute the result psd as the avarage of each FFT result and compute the frequencies
    for(int16_t i=0; i<psd_size; i++){
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



audio_feat_t compute_spec_decrease(audio_sample_t* mags, audio_sample_t* freqs, int16_t len, audio_feat_t sum_mags){

    audio_feat_t sum = 0.0;
    audio_sample_t dc_mag = mags[0];

    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "dc_mag", dc_mag);

    for(int16_t i=0; i<len; i++){
        sum += (mags[i] - dc_mag) / (i + 1);
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "sum", sum);
    audio_feat_t result = sum / sum_mags;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_decrease", "result", result);
    return result;
}


audio_feat_t compute_spectral_slope(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags){

    audio_feat_t mean_mag = sum_mags / len;
    audio_feat_t mean_freq = 0.0;

    mean_freq = vect_mean<audio_feat_t>(freqs, len);

    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "mean_mag", mean_mag);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "mean_freq", mean_freq);

    // Numerator and denominator for the final slope computation
    audio_feat_t num = 0.0;
    audio_feat_t den = 0.0;

    for(int16_t i=0; i<len; i++){
        num += ((audio_feat_t)freqs[i] - mean_freq) * ((audio_feat_t)mags[i] - mean_mag);
        den += ((audio_feat_t)freqs[i] - mean_freq) * ((audio_feat_t)freqs[i] - mean_freq);
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "num", num);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "den", den);
    audio_feat_t result = num / den;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_slope", "result", result);
    return result;
}



audio_feat_t compute_rolloff(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags){

    audio_feat_t rolloff_energy = 0.95 * sum_mags;
    audio_feat_t sum = 0.0;
    audio_feat_t rolloff = -1.0;   // Error value

    RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "rolloff_energy", rolloff_energy);

    for(int16_t i=0; i<len; i++){
        sum += mags[i];
        RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "sum", sum);

        if(sum >= rolloff_energy){
            RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "result", freqs[i]);
            return freqs[i];
        }
    }

    RA_LOG_SCALAR("AUDIO_FFT", "rolloff", "result", rolloff);
    return rolloff;
}



audio_feat_t compute_centroid(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags){

    audio_feat_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        sum += freqs[i] * mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "centroid", "sum", sum);
    audio_feat_t result = sum / sum_mags;
    RA_LOG_SCALAR("AUDIO_FFT", "centroid", "result", result);
    return result;
}


audio_feat_t compute_spread(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags, audio_feat_t centroid){

    audio_feat_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        sum += ((audio_feat_t)freqs[i] - centroid) * ((audio_feat_t)freqs[i] - centroid) * (audio_feat_t)mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spread", "sum", sum);
    audio_sample_t result = sqrtreal(sum / sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spread", "result", result);
    return result;
}



audio_feat_t compute_kurt(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags, audio_feat_t centroid, audio_feat_t spread){

    audio_feat_t spread_4 = spread * spread * spread * spread; // spread^4
    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "spread_4", spread_4);

    audio_feat_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        audio_feat_t tmp = ((audio_feat_t)freqs[i] - centroid) * ((audio_feat_t)freqs[i] - centroid);
        sum += tmp * tmp * (audio_feat_t)mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "sum", sum);
    audio_sample_t result = sum / (spread_4 * sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_kurt", "result", result);
    return result;
}



audio_feat_t compute_skew(audio_sample_t *mags, audio_sample_t *freqs, int16_t len, audio_feat_t sum_mags, audio_feat_t centroid, audio_feat_t spread){

    audio_feat_t spread_3 = spread * spread * spread;
    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "spread_3", spread_3);

    audio_feat_t sum = 0.0;

    for(int16_t i=0; i<len; i++){
        audio_feat_t tmp = ((audio_feat_t)freqs[i] - centroid) * ((audio_feat_t)freqs[i] - centroid);
        sum += tmp * ((audio_feat_t)freqs[i] - centroid) * (audio_feat_t)mags[i];
    }

    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "sum", sum);
    audio_feat_t result = sum / (spread_3 * sum_mags);
    RA_LOG_SCALAR("AUDIO_FFT", "spec_skew", "result", result);
    return result;
}


audio_feat_t compute_flatness(audio_sample_t *x, int16_t len){

    RA_LOG_ARRAY("AUDIO_PSD", "flatness", "input", x, len);

    audio_feat_t gmean = 0.0;  // geometric
    audio_feat_t amean = 0.0;  // arithmetic

    audio_feat_t sum_logs = 0.0;
    audio_feat_t log_val = 0.0;
    for(int16_t i=0; i<len; i++){
        log_val = logreal(x[i]);
        RA_LOG_SCALAR("AUDIO_PSD", "flatness", "log_val", log_val);
        sum_logs += log_val;
    }
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "sum_logs_raw", sum_logs);
    sum_logs = sum_logs / len;

    gmean = expreal(sum_logs);
    amean = vect_mean<audio_feat_t>(x, len);

    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "sum_logs", sum_logs);
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "gmean", gmean);
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "amean", amean);
    audio_sample_t result = gmean / amean;
    RA_LOG_SCALAR("AUDIO_PSD", "flatness", "result", result);
    return result;
}


audio_feat_t compute_std(audio_sample_t *x, int16_t len){

    RA_LOG_ARRAY("AUDIO_PSD", "spec_std", "input", x, len);
    audio_feat_t result = vect_std<audio_feat_t>(x, len);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_std", "result", result);
    return result;

}

audio_feat_t compute_spectral_entropy(audio_sample_t *x, int16_t len){

    RA_LOG_ARRAY("AUDIO_PSD", "spec_entropy", "input", x, len);

    audio_feat_t *tmp = (audio_feat_t*)malloc(len * sizeof(audio_feat_t));
    audio_feat_t sum = vect_sum<audio_feat_t>(x, len);
    vect_div_const(x, len, sum, tmp);
    entropy_calc(tmp, len, 2);

    audio_feat_t result = vect_sum<audio_feat_t>(tmp, len);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_entropy", "sum", sum);
    RA_LOG_SCALAR("AUDIO_PSD", "spec_entropy", "result", result);

    free(tmp);

    return result;
}



audio_feat_t get_domiant_freq(audio_sample_t *psd, audio_sample_t *freqs, int16_t len){

    audio_feat_t result = freqs[vect_max_index(psd, len)];
    RA_LOG_SCALAR("AUDIO_PSD", "dominant_freq", "result", result);
    return result;
}



void normalized_bandpowers(audio_sample_t *psd, audio_sample_t *freqs, int16_t len, const int8_t *psd_selector, audio_feat_t *band_powers){

    RA_LOG_ARRAY("AUDIO_PSD", "bandpowers", "psd_input", psd, len);

    audio_feat_t dx_freq = freqs[1] - freqs[0];
    audio_feat_t total_power = simpson(psd, len, dx_freq);

    RA_LOG_SCALAR("AUDIO_PSD", "bandpowers", "total_power", total_power);

    int16_t start_ind_freq = 0; // index of the first frequency inside the band
    int16_t n_bins = 0;         // number of frequency bins inside the band
    int8_t start_found = 0;     // 1 if the start frequency was found, useful to minimize the if-statements

    audio_feat_t band_power = 0.0;

    //check which PSD bands are needed
    for(int16_t i=0; i<N_PSD; i++){
        if(psd_selector[i] == 1){
            start_found = 0;
            start_ind_freq = 0;
            n_bins = 0;
            for(int16_t j=0; j<len; j++){
                if(!start_found && freqs[j] >= psd_bands[i].start){
                    start_ind_freq = j;
                    start_found = 1;
                }
                if(start_found && freqs[j] <= psd_bands[i].end){
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



void mfcc_computation(const audio_sample_t *x, int16_t len, int16_t n_frames, audio_sample_t *coeffs){

    audio_sample_t *db_power = (audio_sample_t*)malloc((MEL_ROWS * n_frames) * sizeof(audio_sample_t));
    for (int i = 0; i < MEL_ROWS * n_frames; i++) db_power[i] = 0;

    // Mel spectrogram
    mel_spectrogram_full(x, len, n_frames, db_power);

    // Convert the power in dB
    power_to_dB(db_power, (MEL_ROWS * n_frames), db_power);

    // Apply the DCT
    dct_matrix(db_power, MEL_ROWS, n_frames, db_power);

    for(int16_t i=0; i<N_MFCC; i++){
        for(int16_t j=0; j<n_frames; j++){
            coeffs[(i*n_frames) + j] = db_power[(i * n_frames) + j];
        }
    }

    free(db_power);
}


void get_mfcc_features(const audio_sample_t *x, int16_t len, audio_feat_t *mean_mfcc, audio_feat_t *std_mfcc){

    int16_t padded_len = (2 * PAD_LEN) + len;                   // lenght of the padded signal
    int16_t n_frames = ((padded_len - N_FFT) / HOP_LEN) + 1;    // number of frames for the stft

    audio_sample_t *coeffs = (audio_sample_t*)malloc((N_MFCC*n_frames) * sizeof(audio_sample_t));
    mfcc_computation(x, len, n_frames, coeffs);


    for(int16_t i=0; i<N_MFCC; i++){
        mean_mfcc[i] = vect_mean<audio_feat_t>(&coeffs[i*n_frames], n_frames);
        std_mfcc[i] = vect_std<audio_feat_t>(&coeffs[i*n_frames], n_frames);
    }

    free(coeffs);

}


void get_mel_spectrogram_features(const audio_sample_t *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed, audio_feat_t *mean_mel_spectr, audio_feat_t *std_mel_spectr, audio_feat_t *max_mel_spectr, audio_feat_t *entropy_mel_spectr){

    int16_t padded_len = (2 * PAD_LEN) + len;                   // lenght of the padded signal
    int16_t n_frames = ((padded_len - N_FFT) / HOP_LEN) + 1;    // number of frames for the stft

    audio_sample_t *spectrogram = (audio_sample_t*)malloc((n_mels_needed * n_frames) * sizeof(audio_sample_t));
    for (int i = 0; i < n_mels_needed * n_frames; i++) spectrogram[i] = 0;

    // Get the spectrogram
    mel_spectrogram(x, len, n_frames, idx_needed, spectrogram);

    // Stores the dB of the mel spectrogram
    audio_sample_t *mel_dB = (audio_sample_t*)malloc((n_mels_needed * n_frames) * sizeof(audio_sample_t));

    // Convert the power of the spectrogram in dB
    power_to_dB(spectrogram, (n_mels_needed * n_frames), mel_dB);

    // Computes the entropy of the spectrogram
    entropy(spectrogram, n_mels_needed, n_frames, entropy_mel_spectr);

    // Computes the mean, std and maximum value of each MEL bin
    for(int8_t i=0; i<n_mels_needed; i++){
        mean_mel_spectr[i] = vect_mean<audio_feat_t>(&mel_dB[i*n_frames], n_frames);
        std_mel_spectr[i] = vect_std<audio_feat_t>(&mel_dB[i*n_frames], n_frames);
        max_mel_spectr[i] = vect_max_value(&mel_dB[i*n_frames], n_frames);
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
