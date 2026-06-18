#include <stdlib.h>
#include <types.h>

#include <filtering.h>
#include <helpers.h>
#include <filters_parameters.h>
#include <range_analysis.h>



void linear_filer(real_t *sig, int len, const real_t *b, const real_t *a, real_t *zi, real_t *res){

    RA_LOG_ARRAY("AUDIO_EEPD", "linear_filter", "input", sig, len);

    real_t sig_1 = 0.0;
    real_t y_1 = 0.0;

    real_t s_1 = zi[0];
    real_t s_2 = zi[1];

    // First one needs to be done outside the loop in order not to
    // update the states (s_1 and s_2)
    res[0] = b[0] * sig[0] + s_1;
    y_1 = res[0];
    sig_1 = sig[0];

    for(int i=1; i<len; i++){

        // s_1 is updated before and s_2 after. This is because at every
        // iteration we need the filter state 1 (s_1) of the current step and
        // the filter state 2 (s_2) of the previous step.
        // Also by doing so we avoid using 2 extra variables for the signal
        // and output values of 2 steps before

        s_1 = b[1] * sig_1 - a[1] * y_1 + s_2;
        res[i] = b[0] * sig[i] + s_1;
        s_2 = b[2] * sig_1 - a[2] * y_1;

        sig_1 = sig[i];
        y_1 = res[i];
    }

    RA_LOG_ARRAY("AUDIO_EEPD", "linear_filter", "output", res, len);
}


void filtfilt(const real_t *sig, int len, const real_t *b, const real_t* a, const real_t *zi, real_t *res){

    RA_LOG_ARRAY("AUDIO_EEPD", "filtfilt", "input", sig, len);

    // PADDING //
    int padded_len = (2 * PADLEN) + len;
    real_t *pad = (real_t*)malloc(padded_len * sizeof(real_t));
    padding(sig, len, PADLEN, pad);

    // GET INITIAL STATE //
    real_t x0 = pad[0];

    // FILTER FORWARD //
    real_t *intermediate = (real_t*)malloc(padded_len * sizeof(real_t)); // intermediate output, cannot be res because it's not padded

    real_t *initial = (real_t*)malloc(2 * sizeof(real_t));
    initial[0] = zi[0] * x0;
    initial[1] = zi[1] * x0;
    linear_filer(pad, padded_len, b, a, initial, intermediate);

    // FILTER BACKWARD //
    real_t *reverse = (real_t*)malloc(padded_len * sizeof(real_t));    // for the reverse filtering
    for(int i=0; i<padded_len; i++){
        reverse[i] = intermediate[padded_len-1-i];
    }

    initial[0] = zi[0] * reverse[0];
    initial[1] = zi[1] * reverse[0];

    real_t *res_padded = (real_t*)malloc(padded_len * sizeof(real_t));
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
