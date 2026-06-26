#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


#include <main.h>
#include <helpers.h>

#include <fsm_control.h>
#include <feature_extraction.h>
#include <audio_features.h>
#include <imu_features.h>
#include <postprocessing.h>

#ifndef FXP_MODE
// define model data for extern purpose; init is done later with read_flash
// these variables cannot be 'static' which causes duplication
// because linker cannot optimize/deduplicate non-const static data in CARUS
#include <audio_model.h>
#include <imu_model.h>

real_t CARUS01 audio_scores[AUD_N_TREES][MAX_LEAVES];
real_t CARUS01 audio_values_comp[AUD_N_TREES][AUD_MAX_NODES];
int16_t CARUS00 audio_feat_comp[AUD_N_TREES][AUD_MAX_NODES];
bigreal_t CARUS11 imu_scores[IMU_N_TREES][IMU_MAX_LEAVES];
bigreal_t CARUS11 imu_values_comp[IMU_N_TREES][IMU_MAX_NODES];
int16_t CARUS10 imu_feat_comp[IMU_N_TREES][IMU_MAX_NODES];
#endif

#ifdef FXP_MODE
#include <FxP/core/fxp_core.h>
typedef fxp_q16_t score_t;
#define SCORE_THRESHOLD_AUDIO ((score_t)FXP_AUDIO_SCORE_TH_Q16)
#define SCORE_THRESHOLD_IMU ((score_t)FXP_IMU_SCORE_TH_Q16)
#else
typedef feat_t score_t;
#define SCORE_THRESHOLD_AUDIO ((score_t)AUDIO_TH)
#define SCORE_THRESHOLD_IMU ((bigreal_t)IMU_TH)
#endif

template<typename T>
static inline uint8_t is_cough(T score, T threshold)
{
    return (score >= threshold) ? 1U : 0U;
}

#ifdef HEEPATIA_MODE
// store heap data in gcram (in this case, virtual gcram for fpga purpose)
// this only works with the modified linker in this branch
#include <sys/types.h>
#include <errno.h>
extern char __virtgcram_heap_start[];
extern char __virtgcram_heap_end[];

static char *current_heap_ptr = __virtgcram_heap_start;

#ifdef __cplusplus
extern "C" {
#endif
#include <coprosit_cpu.h>

void *__wrap__sbrk(ptrdiff_t incr) {
    char *prev_heap_ptr = current_heap_ptr;

    if (current_heap_ptr + incr > __virtgcram_heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }

    current_heap_ptr += incr;
    return (void *)prev_heap_ptr;
}

#ifndef LPOS_MODE
// for some reason builtin errno is gone when not compiling for LPOS_MODE
int *__errno(void) {
    static int errno_val = 0;
    return &errno_val;
}
#endif

// putchar is also gone ffs
int putchar(int c) {
    int (*volatile stdout_printf)(const char *, ...) = printf;
    return stdout_printf("%c", (char)c) >= 0 ? c : EOF;
}
#ifdef __cplusplus
}
#endif
#endif

#if defined(FXP_MODE) && defined(HEEPATIA_MODE)
#error "FXP_MODE and HEEPATIA_MODE are mutually exclusive: FxP is not supported on HEEPatia"
#endif

#if defined(LPOS_MODE) && !defined(HEEPATIA_MODE)
#error "LPOS_MODE requires HEEPATIA_MODE: libposit is only available on HEEPatia"
#endif

volatile int posit_done;
volatile int posit_ok;

int launch(void)
{
#ifdef HEEPATIA_MODE
    if (w25q128jw_init(spi_flash) != FLASH_OK){
        printf("FLASH INIT ERROR\n");
        return EXIT_FAILURE;
    }
#endif

#ifndef FXP_MODE
    // initialize audio/imu model data from FLASH because CARUS are NOLOAD
    // for non-heepatia, this is equivalent to memcpy (which is not efficient but consistent)
    read_flash(audio_scores_src, audio_scores, sizeof(audio_scores));
    read_flash(audio_values_comp_src, audio_values_comp, sizeof(audio_values_comp));
    read_flash(audio_feat_comp_src, audio_feat_comp, sizeof(audio_feat_comp));
    read_flash(imu_scores_src, imu_scores, sizeof(imu_scores));
    read_flash(imu_values_comp_src, imu_values_comp, sizeof(imu_values_comp));
    read_flash(imu_feat_comp_src, imu_feat_comp, sizeof(imu_feat_comp));
#endif

    int16_t *indexes_audio_f = (int16_t *)malloc((size_t)N_AUDIO_FEATURES * sizeof(int16_t));
    int8_t *indexes_imu_f = (int8_t *)malloc((size_t)N_IMU_FEATURES * sizeof(int8_t));

    int16_t idx = 0;
    for (int16_t i = 0; i < Number_AUDIO_Features; i++) {
        if (audio_features_selector[i] == 1) {
            indexes_audio_f[idx] = i;
            idx++;
        }
    }
    idx = 0;
    for (int8_t i = 0; i < Number_IMU_Features; i++) {
        if (imu_features_selector[i] == 1) {
            indexes_imu_f[idx] = i;
            idx++;
        }
    }

    feat_t *audio_feature_array = (feat_t *)malloc((size_t)Number_AUDIO_Features * sizeof(feat_t));
    for (int i = 0; i < Number_AUDIO_Features; i++) audio_feature_array[i] = 0;

    bigreal_t *imu_feature_array = (bigreal_t *)malloc((size_t)Number_IMU_Features * sizeof(bigreal_t));
    for (int i = 0; i < Number_IMU_Features; i++) imu_feature_array[i] = 0;

    feat_t *features_audio_model = (feat_t *)malloc((size_t)TOT_FEATURES_AUDIO_MODEL_AUDIO * sizeof(feat_t));
    bigreal_t *features_imu_model = (bigreal_t *)malloc((size_t)TOT_FEATURES_IMU_MODEL_IMU * sizeof(bigreal_t));

    score_t audio_score = 0;
    bigreal_t imu_score = 0;

    uint16_t *starts = (uint16_t *)malloc((size_t)MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    uint16_t *ends = (uint16_t *)malloc((size_t)MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    uint16_t *locs = (uint16_t *)malloc((size_t)MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    postproc_peak_t *peaks = (postproc_peak_t *)malloc((size_t)MAX_PEAKS_EXPECTED * sizeof(postproc_peak_t));

    uint16_t n_peaks = 0;
    uint16_t new_added = 0;

    score_t *audio_confidence = (score_t *)malloc((size_t)MAX_PEAKS_EXPECTED * sizeof(score_t));

    uint32_t idx_start_window = 0;
    uint16_t n_idxs_above_th = 0;
    int debug_cnt = 0;

    feat_t gender_feature = 0;
    feat_t bmi_feature = 0;

#ifdef FXP_MODE
    int16_t *audio = (int16_t *)malloc((size_t)AUDIO_LEN * sizeof(int16_t));
    q11_5_t (*imu)[Num_IMU_signals] = (q11_5_t(*)[Num_IMU_signals])malloc((size_t)IMU_LEN * sizeof(*imu));
    const audio_sample_t *audio_runtime_in = NULL;
    const imu_sample_t (*imu_runtime_in)[Num_IMU_signals] = NULL;

    if (!audio || !imu) {
        free(audio);
        free(imu);
        free(indexes_audio_f);
        free(indexes_imu_f);
        free(audio_feature_array);
        free(imu_feature_array);
        free(features_audio_model);
        free(features_imu_model);
        free(starts);
        free(ends);
        free(locs);
        free(peaks);
        free(audio_confidence);

#ifdef LPOS_MODE
        posit_done = 1;
        posit_ok = 0;
#endif

        return 1;
    }

    /* Single runtime boundary: source float samples are converted once to FxP carriers. */
    for (int32_t i = 0; i < AUDIO_LEN; i++) {
        audio[i] = cough_source_audio_sample(audio_in.air[i]);
    }
    for (int32_t i = 0; i < IMU_LEN; i++) {
        for (int8_t ax = 0; ax < Num_IMU_signals; ax++) {
            imu[i][ax] = cough_source_imu_sample(imu_in[i][ax]);
        }
    }

    audio_runtime_in = audio;
    imu_runtime_in = imu;
    gender_feature = cough_source_feat(gender);
    bmi_feature = cough_source_feat(bmi);
#else
    // for non-fxp, we route all data through `read_flash` for consistency
    audio_sample_t *audio_buf = (real_t *)malloc(WINDOW_SAMP_AUDIO * sizeof(real_t));
    bigreal_t (*imu_buf)[Num_IMU_signals] = (bigreal_t (*)[Num_IMU_signals])malloc(WINDOW_SAMP_IMU * Num_IMU_signals * sizeof(bigreal_t));
    gender_feature = (bigreal_t)gender;
    bmi_feature = (bigreal_t)bmi;
#endif

    init_state();

    while (1) {
        idx_start_window = get_idx_window();
        printf("PROCESS WINDOW %d WITH MODEL %d\n", idx_start_window, fsm_state.model);

        if (fsm_state.model == IMU_MODEL) {
            if (idx_start_window + WINDOW_SAMP_IMU >= IMU_LEN) {
                init_state();
                idx_start_window = get_idx_window();
            }

#ifdef FXP_MODE
            const imu_sample_t (*imu_signal)[Num_IMU_signals] = &imu_runtime_in[idx_start_window];
#else
            const bigreal_t (*imu_signal)[Num_IMU_signals] = imu_buf;
            read_flash(&imu_in[idx_start_window], imu_buf, WINDOW_SAMP_IMU * Num_IMU_signals * sizeof(bigreal_t));
#endif

            imu_features(imu_features_selector,
                         imu_signal,
                         WINDOW_SAMP_IMU,
                         imu_feature_array);

            for (int16_t j = 0; j < N_IMU_FEATURES; j++) {
                features_imu_model[j] = imu_feature_array[indexes_imu_f[j]];
            }
            if (imu_bio_feats_selector[0] == 1) {
                features_imu_model[N_IMU_FEATURES] = gender_feature;
            }
            if (imu_bio_feats_selector[1] == 1) {
                features_imu_model[N_IMU_FEATURES + 1] = bmi_feature;
            }

            printf("IMU PREDICT\n");
            imu_score = imu_predict(features_imu_model);
            printf("IMU PROB:"); print_float((float)imu_score, 8); printf("\n");
            fsm_state.model_cls_out = is_cough(imu_score, SCORE_THRESHOLD_IMU) ? COUGH_OUT : NON_COUGH_OUT;
        } else {
            if (idx_start_window + WINDOW_SAMP_AUDIO >= AUDIO_LEN) {
                break;
            }

#ifdef FXP_MODE
            const audio_sample_t *audio_signal = &audio_runtime_in[idx_start_window];
#else
            const audio_sample_t *audio_signal = audio_buf;
            read_flash(&audio_in.air[idx_start_window], audio_buf, WINDOW_SAMP_AUDIO * sizeof(real_t));
#endif

            audio_features(audio_features_selector,
                           audio_signal,
                           WINDOW_SAMP_AUDIO,
                           AUDIO_FS,
                           audio_feature_array);

            for (int16_t j = 0; j < N_AUDIO_FEATURES; j++) {
                features_audio_model[j] = audio_feature_array[indexes_audio_f[j]];
            }
            if (audio_bio_feats_selector[0] == 1) {
                features_audio_model[N_AUDIO_FEATURES] = gender_feature;
            }
            if (audio_bio_feats_selector[1] == 1) {
                features_audio_model[N_AUDIO_FEATURES + 1] = bmi_feature;
            }

            printf("AUDIO PREDICT\n");
            audio_score = audio_predict(features_audio_model);
            printf("AUDIO PROB:"); print_float((float)audio_score, 8); printf("\n");
            fsm_state.model_cls_out = is_cough(audio_score, SCORE_THRESHOLD_AUDIO) ? COUGH_OUT : NON_COUGH_OUT;

            _get_cough_peaks(audio_signal, WINDOW_SAMP_AUDIO, AUDIO_FS,
                             &starts[n_peaks], &ends[n_peaks], &locs[n_peaks], &peaks[n_peaks], &new_added);

            for (uint16_t j = 0; j < new_added; j++) {
                starts[n_peaks + j] += idx_start_window;
                ends[n_peaks + j] += idx_start_window;
                locs[n_peaks + j] += idx_start_window;
                audio_confidence[n_peaks + j] = audio_score;
            }
            n_peaks += new_added;
        }

        update();

        if (check_postprocessing()) {
            uint16_t n_peaks_final = 0;

            if (n_peaks > 0) {
                uint16_t *idxs_above_th = (uint16_t *)malloc((size_t)n_peaks * sizeof(uint16_t));

                for (uint16_t i = 0; i < n_peaks; i++) {
                    if (is_cough(audio_confidence[i], SCORE_THRESHOLD_AUDIO)) {
                        idxs_above_th[n_idxs_above_th] = i;
                        n_idxs_above_th++;
                    }
                }

                uint16_t *final_starts = (uint16_t *)malloc((size_t)n_idxs_above_th * sizeof(uint16_t));
                uint16_t *final_ends = (uint16_t *)malloc((size_t)n_idxs_above_th * sizeof(uint16_t));
                uint16_t *above_locs = (uint16_t *)malloc((size_t)n_idxs_above_th * sizeof(uint16_t));
                postproc_peak_t *above_peaks = (postproc_peak_t *)malloc((size_t)n_idxs_above_th * sizeof(postproc_peak_t));

                for (uint16_t i = 0; i < n_idxs_above_th; i++) {
                    final_starts[i] = starts[idxs_above_th[i]];
                    final_ends[i] = ends[idxs_above_th[i]];
                    above_locs[i] = locs[idxs_above_th[i]];
                    above_peaks[i] = peaks[idxs_above_th[i]];
                }

                n_peaks_final = _clean_cough_segments(final_starts, final_ends, above_locs, above_peaks, n_idxs_above_th, AUDIO_FS);

#ifdef EVALUATION_MODE
                for (uint16_t k = 0; k < n_peaks_final; k++) {
                    printf("COUGH_SEG: %u %u\n", final_starts[k], final_ends[k]);
                }
#endif

                free(idxs_above_th);
                free(final_starts);
                free(final_ends);
                free(above_locs);
                free(above_peaks);
            }

#ifdef EVALUATION_MODE
            printf("N_PEAKS FINAL: %d\n", n_peaks_final);
#endif
            (void)n_peaks_final;

            n_peaks = 0;
            n_idxs_above_th = 0;
        }

        debug_cnt++;
        if (debug_cnt == 100) {
            break;
        }
    }

    free(indexes_audio_f);
    free(indexes_imu_f);
    free(audio_feature_array);
    free(imu_feature_array);
    free(features_audio_model);
    free(features_imu_model);
    free(starts);
    free(ends);
    free(locs);
    free(peaks);
    free(audio_confidence);

#ifdef FXP_MODE
    free(audio);
    free(imu);
#endif

#ifdef LPOS_MODE
    posit_ok = 1;
    posit_done = 1;
#endif

    return 0;
}

int main(void){
#ifdef LPOS_MODE
    posit_ok = 0;

    coprosit_launch_cpu((coprosit_start_function_ptr_t)&launch);
    while (!posit_done);
    coprosit_reset_cpu();

    return 1 - posit_ok;
#else
    return launch();
#endif
}
