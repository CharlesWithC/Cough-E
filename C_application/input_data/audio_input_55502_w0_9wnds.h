#ifndef _AUDIO_INPUT_55502_W2_H_
#define _AUDIO_INPUT_55502_W2_H_

#include <types.h>

/* Sampling frequency */
#define AUDIO_FS  8000

/* Number of samples per each audiosignal */
#define AUDIO_LEN   57600


/*
	The samples are taken from two microphones, one facing the sking
	the other facing the air
    In this case only the air is used, this is a simplified input
    data set
*/
typedef struct audio_input_55502
{
    num_t air[AUDIO_LEN];
} audio_input_t;

extern audio_input_t audio_in;

#endif
