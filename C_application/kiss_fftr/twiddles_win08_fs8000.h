#ifndef _TWIDDLES_H_
#define _TWIDDLES_H_

#include <types.h>

typedef struct twiddles_entry
{
	num_t cosine;
	num_t sine;
} twiddles_t;

extern twiddles_t twiddles_3200[3200];
extern twiddles_t twiddles_450[450];
extern twiddles_t twiddles_1024[1024];
extern twiddles_t twiddles_1600[1600];
extern twiddles_t twiddles_225[225];
extern twiddles_t twiddles_512[512];

#endif
