/*Author: Alessio*/
#ifndef CONTROL_H
#define CONTROL_H

/*Macros used by the control loops*/
#define RSHIFT(num,shift) ( (num) >> (shift) )
#define LSHIFT(num,shift) ( (num) << (shift) )
/*Same but more readable*/
#define INCREASE_SHIFT(num,shift) ( (num) << (shift) )
#define DECREASE_SHIFT(num,shift) ( (num) >> (shift) )

#define SENSOR_PRECISION 10

#endif
