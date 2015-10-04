#ifndef MOTORS_H
#define MOTORS_H

#include "x32.h"

#define X32_QR_a0 		peripherals[PERIPHERAL_XUFO_A0]
#define X32_QR_a1 		peripherals[PERIPHERAL_XUFO_A1]
#define X32_QR_a2 		peripherals[PERIPHERAL_XUFO_A2]
#define X32_QR_a3 		peripherals[PERIPHERAL_XUFO_A3]

int32_t offset[4] = 0;
int32_t rpm[4] = 0;

/* Add offset to the four motors
 * No need to check for negative numbers since offset can be negative
 * Author: Bastiaan
 */
void add_motor_offset(int32_t motor0, int32_t motor1, int32_t motor2, int32_t motor3) {
	offset[0] += motor0;
	offset[1] += motor1;
	offset[2] += motor2;
	offset[3] += motor3;
}

int32_t get_motor_offset(int32_t i) {
	if(i > 3) return 0;
	return offset[i];
}

void set_motor_rpm(int32_t motor0, int32_t motor1, int32_t motor2, int32_t motor3) {
	int32_t i;

	rpm[0] = motor0;
	rpm[1] = motor1;
	rpm[2] = motor2;
	rpm[3] = motor3;

	// Clip values to be below the max
	for(i = 0; i < 4; i++)
		rpm[i] = rpm[i] & 0x3ff;

	X32_QR_a0 = rpm[0];
	X32_QR_a1 = rpm[1];
	X32_QR_a2 = rpm[2];
	X32_QR_a3 = rpm[3];

	/*if(T>0){ TODO: This logic does not belong here
		motor0 = ((motor0 < T/4 ? T/4 : motor0) > 0x3ff)? 0x3ff : motor0;
		motor1 = ((motor1 < T/4 ? T/4 : motor1) > 0x3ff)? 0x3ff : motor1;
		motor2 = ((motor2 < T/4 ? T/4 : motor2) > 0x3ff)? 0x3ff : motor2;
		motor3 = ((motor3 < T/4 ? T/4 : motor3) > 0x3ff)? 0x3ff : motor3;
	}*/
}

/* Reset the offsets.
   Author: Alessio
*/
void reset_motors() {
	offset[0] = offset[1] = offset[2] = offset[3] = 0;
	set_motor_rpm(0,0,0,0);
}


/* Gets the RPM of a certain motor.
 * Author: Bastiaan
 */
int32_t get_motor_rpm(int32_t i) {
	if(i > 3) return 0;
	return rpm[i];
}

#endif
