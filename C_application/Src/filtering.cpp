#include <stdlib.h>
#include <types.h>

#include <filtering.h>
#include <helpers.h>
#include <filters_parameters.h>
#include <range_analysis.h>

void linear_filer(audio_sample_t *sig, int len, const audio_sample_t *b, const audio_sample_t *a, audio_sample_t *zi, audio_sample_t *res){

    RA_LOG_ARRAY("AUDIO_EEPD", "linear_filter", "input", sig, len);

    audio_sample_t sig_1 = 0.0;
    audio_sample_t y_1 = 0.0;

    audio_sample_t s_1 = zi[0];
    audio_sample_t s_2 = zi[1];

    // First one needs to be done outside the loop in order not to
    // update the states (s_1 and s_2)
    res[0] = b[0] * audio_sample_t(sig[0]) + s_1;
    y_1 = res[0];
    sig_1 = sig[0];

    for(int i=1; i<len; i++){

        // s_1 is updated before and s_2 after. This is because at every
        // iteration we need the filter state 1 (s_1) of the current step and
        // the filter state 2 (s_2) of the previous step.
        // Also by doing so we avoid using 2 extra variables for the signal
        // and output values of 2 steps before

        s_1 = b[1] * audio_sample_t(sig_1) - a[1] * y_1 + s_2;
        res[i] = b[0] * audio_sample_t(sig[i]) + s_1;
        s_2 = b[2] * audio_sample_t(sig_1) - a[2] * y_1;

        sig_1 = sig[i];
        y_1 = res[i];
    }

    RA_LOG_ARRAY("AUDIO_EEPD", "linear_filter", "output", res, len);
}


void filtfilt(const audio_sample_t *sig, int len, const audio_sample_t *b, const audio_sample_t* a, const audio_sample_t *zi, audio_sample_t *res){

    RA_LOG_ARRAY("AUDIO_EEPD", "filtfilt", "input", sig, len);

    // PADDING //
    int padded_len = (2 * PADLEN) + len;
    audio_sample_t *pad = (audio_sample_t*)malloc(padded_len * sizeof(audio_sample_t));
    padding(sig, len, PADLEN, pad);

    // GET INITIAL STATE //
    audio_sample_t x0 = pad[0];

    // FILTER FORWARD //
    audio_sample_t *intermediate = (audio_sample_t*)malloc(padded_len * sizeof(audio_sample_t)); // intermediate output, cannot be res because it's not padded

    audio_sample_t *initial = (audio_sample_t*)malloc(2 * sizeof(audio_sample_t));
    initial[0] = zi[0] * audio_sample_t(x0);
    initial[1] = zi[1] * audio_sample_t(x0);
    linear_filer(pad, padded_len, b, a, initial, intermediate);

    // FILTER BACKWARD //
    audio_sample_t *reverse = (audio_sample_t*)malloc(padded_len * sizeof(audio_sample_t));    // for the reverse filtering
    for(int i=0; i<padded_len; i++){
        reverse[i] = (audio_sample_t)intermediate[padded_len-1-i];
    }

    initial[0] = zi[0] * audio_sample_t(reverse[0]);
    initial[1] = zi[1] * audio_sample_t(reverse[0]);

    audio_sample_t *res_padded = (audio_sample_t*)malloc(padded_len * sizeof(audio_sample_t));
    linear_filer(reverse, padded_len, b, a, initial, res_padded);

    // CUT THE PADDING TO GET THE FINAL RESULT //
    for(int i=0; i<len; i++){
        res[i] = res_padded[PADLEN+len-1-i];    // cut the padding and reverse
    }

    RA_LOG_ARRAY("AUDIO_EEPD", "filtfilt", "output", res, len);

    free(pad);
    free(intermediate);
    free(res_padded);
    free(initial);
    free(reverse);
}
