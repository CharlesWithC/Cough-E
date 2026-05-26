#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <helpers.h>

#include <types.h>
#include <audio_model.h>
#include <range_analysis.h>

/// @brief Computes the sigmoid value of a given score
/// @param score    :   the score for which to compute the sigmoid
/// @return The resulting value
num_t _audio_sigmoid(num_t score){
    if(score < 0.0){
        num_t z = expnum(score);
        return z / (1.0 + z);
    }
    return (1.0 / (1.0 + expnum(-score)));
}



num_t audio_predict(num_t *feats){

    RA_LOG_ARRAY("CLASSIFY", "audio_predict", "feats_input", feats, TOT_FEATURES_AUDIO_MODEL_AUDIO);

    num_t score = 0.0;

    int16_t current_node = 0;
    int16_t child_type = 0;

    for(int16_t t=0; t<AUD_N_TREES; t++){

        current_node = 0;
        child_type = 0;

        for(int16_t n=0; n<AUD_MAX_NODES; n++){

            if(feats[audio_feat_comp[t][current_node]] < audio_values_comp[t][current_node]){
                child_type = audio_children[t][current_node].child_left.type;
                current_node = audio_children[t][current_node].child_left.id;
            } else {
                child_type = audio_children[t][current_node].child_right.type;
                current_node = audio_children[t][current_node].child_right.id;
            }

            if(child_type == AUD_LEAF_T){
                score += audio_scores[t][current_node];
                break;
            }
        }
    }

    RA_LOG_SCALAR("CLASSIFY", "audio_predict", "score", score);

    num_t res = _audio_sigmoid(score);
    RA_LOG_SCALAR("CLASSIFY", "_audio_sigmoid", "result", res);

    return res;
}
