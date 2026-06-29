#ifndef _AUDIO_INPUT_55502_W2_H_
#define _AUDIO_INPUT_55502_W2_H_

#include <types.h>

/* Sampling frequency */
#define AUDIO_FS 8000

/* Number of samples per each audiosignal */
#define AUDIO_LEN 57600

#ifndef FXP_MODE
#define AUDIO_INPUT_TYPE cough_audio_sample_t
#else
#define AUDIO_INPUT_TYPE float
#endif

/*
        The samples are taken from two microphones, one facing the sking
        the other facing the air
    In this case only the air is used, this is a simplified input
    data set
*/
typedef struct audio_input_55502 {
    AUDIO_INPUT_TYPE air[AUDIO_LEN];
} audio_input_t;

extern const audio_input_t FLASH2 audio_in;

#endif
