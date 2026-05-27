#ifndef _IMU_INPUT_55502_H_
#define _IMU_INPUT_55502_H_

#include <types.h>

/* Sampling frequency of the IMU signal */
#define IMU_FS 100

/*
	Number of samples for the IMU signals
	Note that each sample contains the meaasurement of each of
	the three axis of the accelerometer and gyroscope.
*/
#define IMU_LEN 720



/*
	Each sub-array contains one sample per each IMU signal
	in the following order:
	{
		Accelerometer_x,
		Accelerometer_y,
		Accelerometer_z,
		Gyroscope_z,
		Gyroscope_p,
		Gyroscope_r
	}
*/

extern num_t imu_in[IMU_LEN][6];

#endif
