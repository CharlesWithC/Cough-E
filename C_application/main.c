#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef USE_FLASH
#include <w25q128jw.h>
#endif

#include <main.h>

#include <types.h>
#include <fsm_control.h>
#include <feature_extraction.h>
#include <audio_features.h>
#include <imu_features.h>
#include <postprocessing.h>



int main(){

    // These two arrays contain the indexes of the features that are going to be extracted
    int16_t *indexes_audio_f = (int16_t*)malloc(N_AUDIO_FEATURES * sizeof(int16_t));
    int8_t *indexes_imu_f = (int8_t*)malloc(N_IMU_FEATURES * sizeof(int8_t));

    int16_t idx = 0;
    for(int16_t i=0; i<Number_AUDIO_Features; i++){
        if(audio_features_selector[i] == 1){
            indexes_audio_f[idx] = i;
            idx++;
        }
    }
    idx = 0;
    for(int8_t i=0; i<Number_IMU_Features; i++){
        if(imu_features_selector[i] == 1){
            indexes_imu_f[idx] = i;
            idx++;
        }
    }

    ////    AUDIO FEATURES    ////
    // Array for containing the audio features values. The order is the same as of the
    // features families enum
    num_t  *audio_feature_array = (num_t*)malloc(Number_AUDIO_Features * sizeof(num_t));
    memset(audio_feature_array, 0.0, Number_AUDIO_Features);


    ////    IMU FEATURES    ////
    num_t *imu_feature_array = (num_t*)malloc(Number_IMU_Features * sizeof(num_t)); // To store all the possible IMU features
    memset(imu_feature_array, 0.0, Number_IMU_Features);

    // Array for the features set of the audio model
    num_t* features_audio_model = (num_t*)malloc(TOT_FEATURES_AUDIO_MODEL_AUDIO * sizeof(num_t));

    num_t audio_proba = 0.0;

    // Array for the features set of the imu model
    num_t* features_imu_model = (num_t*)malloc(TOT_FEATURES_IMU_MODEL_IMU * sizeof(num_t));

    num_t imu_proba = 0.0;

    // Postprocessing arrays and variables
    uint16_t *starts = (uint16_t*)malloc(MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    uint16_t *ends = (uint16_t*)malloc(MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    uint16_t *locs = (uint16_t*)malloc(MAX_PEAKS_EXPECTED * sizeof(uint16_t));
    num_t *peaks = (num_t*)malloc(MAX_PEAKS_EXPECTED * sizeof(num_t));

    // Number of peaks found from last output
    uint16_t n_peaks = 0;

    // Number of peaks found in the current window
    uint16_t new_added = 0;

    // Confidence of the model per each peak found
    num_t *audio_confidence = (num_t*)malloc(MAX_PEAKS_EXPECTED * sizeof(num_t*));

    // Index of the start of the current window (depending on the model to use, it indexes the AUDIO or the IMU signal)
    uint32_t idx_start_window = 0;

    // Number of peaks for which the model confidence is above the threshold
    uint16_t n_idxs_above_th = 0;

    int debug_cnt = 0;  // Used to count the iterations and eventually stop

    init_state();


    // Looping through the windows
    while(1){

        idx_start_window = get_idx_window();
        // printf("Start: %d\n", idx_start_window);

        if(fsm_state.model == IMU_MODEL){

            if(idx_start_window >= IMU_LEN){
                // printf("RESET\n");
                init_state();
                idx_start_window = get_idx_window();
            }

            // Extract IMU features
            #ifdef USE_FLASH
            num_t *buffer = (num_t*)malloc(WINDOW_SAMP_IMU*Num_IMU_signals*sizeof(num_t));
            uint32_t source_flash = (uint32_t)heep_get_flash_address_offset((uint32_t *)&imu_in[idx_start_window]);
            if(w25q128jw_read_standard(source_flash, buffer, WINDOW_SAMP_IMU*Num_IMU_signals*sizeof(num_t))!=FLASH_OK) printf("Error reading from flash\n");

            imu_features(imu_features_selector, (num_t (*)[Num_IMU_signals])buffer, WINDOW_SAMP_IMU, imu_feature_array);
            #else
            imu_features(imu_features_selector, &imu_in[idx_start_window], WINDOW_SAMP_IMU, imu_feature_array);
            #endif

            // Fill the array of final imu features to feed into the IMU model
            for(int16_t j=0; j<N_IMU_FEATURES; j++){
                features_imu_model[j] = imu_feature_array[indexes_imu_f[j]];
                // printf("[%d]\t%f\n", j, features_imu_model[j]);
            }
            if(imu_bio_feats_selector[0] == 1){
                features_imu_model[N_IMU_FEATURES] = gender;
            }
            if(imu_bio_feats_selector[1] == 1){
                features_imu_model[N_IMU_FEATURES+1] = bmi;
            }

            // Predict with the IMU model
            imu_proba = imu_predict(features_imu_model);
            // #ifdef USE_UNUM_POSIT
            // std::cout<<"IMU P: "<<std::fixed<<std::setprecision(6)<<imu_proba<<"\n";
            // #else
            // printf("IMU P: %f\n", imu_proba);
            // #endif

            // Update the output of the FSM
            if(imu_proba>=IMU_TH){
                fsm_state.model_cls_out = COUGH_OUT;
            } else {
                fsm_state.model_cls_out = NON_COUGH_OUT;
            }
        }
        else {

            if(idx_start_window >= AUDIO_LEN){
                break;
                init_state();
                idx_start_window = get_idx_window();
            }

            // Extract AUDIO features
            #ifdef USE_FLASH
            num_t *buffer = (num_t*)malloc(WINDOW_SAMP_AUDIO*sizeof(num_t));
            uint32_t source_flash = (uint32_t)heep_get_flash_address_offset((uint32_t *)&audio_in.air[idx_start_window]);
            if(w25q128jw_read_standard(source_flash, buffer, WINDOW_SAMP_AUDIO*sizeof(num_t))!=FLASH_OK) printf("Error reading from flash\n");

            audio_features(audio_features_selector, buffer, WINDOW_SAMP_AUDIO, AUDIO_FS, audio_feature_array);
            #else
            audio_features(audio_features_selector, &audio_in.air[idx_start_window], WINDOW_SAMP_AUDIO, AUDIO_FS, audio_feature_array);
            #endif

            // Fill the array of fifeatures_imu_modelnal audio features to feed into the AUDIO model
            for(int16_t j=0; j<N_AUDIO_FEATURES; j++){
                features_audio_model[j] = audio_feature_array[indexes_audio_f[j]];
            }
            if(audio_bio_feats_selector[0] == 1){
                features_audio_model[N_AUDIO_FEATURES] = gender;
            }
            if(audio_bio_feats_selector[1] == 1){
                features_audio_model[N_AUDIO_FEATURES+1] = bmi;
            }

            audio_proba = audio_predict(features_audio_model);
            // #ifdef USE_UNUM_POSIT
            // std::cout<<"AUDIO P: "<<std::fixed<<std::setprecision(6)<<audio_proba<<"\n";
            // #else
            // printf("AUDIO P: %f\n", audio_proba);
            // #endif

            // Update the output of the FSM
            if(audio_proba >= AUDIO_TH){
                fsm_state.model_cls_out = COUGH_OUT;
            } else {
                fsm_state.model_cls_out = NON_COUGH_OUT;
            }

            // Identify the peaks
            #ifdef USE_FLASH
            _get_cough_peaks(buffer, WINDOW_SAMP_AUDIO, AUDIO_FS, &starts[n_peaks], &ends[n_peaks], &locs[n_peaks], &peaks[n_peaks], &new_added);
            #else
            _get_cough_peaks(&audio_in.air[idx_start_window], WINDOW_SAMP_AUDIO, AUDIO_FS, &starts[n_peaks], &ends[n_peaks], &locs[n_peaks], &peaks[n_peaks], &new_added);
            #endif

            for(uint16_t j=0; j<new_added; j++){
                starts[n_peaks+j] += idx_start_window;
                ends[n_peaks+j] += idx_start_window;
                locs[n_peaks+j] += idx_start_window;
                audio_confidence[n_peaks+j] = audio_proba;
            }
            n_peaks += new_added;
        }

        update();

        if(check_postprocessing()){

            uint16_t n_peaks_final = 0;

            if(n_peaks > 0){
                // Keeps track of the indexes of the peaks for which the model confidence is above the threshold
                uint16_t *idxs_above_th = (uint16_t*)malloc(n_peaks * sizeof(uint16_t));


                for(uint16_t i=0; i<n_peaks; i++){
                    if(audio_confidence[i] >= AUDIO_TH){
                        idxs_above_th[n_idxs_above_th] = i;
                        n_idxs_above_th++;
                    }
                }

                uint16_t *final_starts = (uint16_t*)malloc(n_idxs_above_th * sizeof(uint16_t));
                uint16_t *final_ends = (uint16_t*)malloc(n_idxs_above_th * sizeof(uint16_t));
                uint16_t *above_locs = (uint16_t*)malloc(n_idxs_above_th * sizeof(uint16_t));
                num_t *above_peaks = (num_t*)malloc(n_idxs_above_th * sizeof(num_t));


                for(uint16_t i=0; i<n_idxs_above_th; i++){
                    final_starts[i] = starts[idxs_above_th[i]];
                    final_ends[i] = ends[idxs_above_th[i]];
                    above_locs[i] = locs[idxs_above_th[i]];
                    above_peaks[i] = peaks[idxs_above_th[i]];
                }

                // TODO: stai passando due volte gli stessi parametri!
                n_peaks_final = _clean_cough_segments(final_starts, final_ends, above_locs, above_peaks, n_idxs_above_th, AUDIO_FS);

                #ifdef EVALUATION_MODE
                for(uint16_t k=0; k<n_peaks_final; k++){
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

            // Reset postprocessing variables to their default value
            n_peaks = 0;

            n_idxs_above_th = 0;
        }

        debug_cnt++;

        if(debug_cnt == 100){
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


    return 0;
}
