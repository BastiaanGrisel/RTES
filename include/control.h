/*Author: Alessio*/
#ifndef CONTROL_H
#define CONTROL_H
/*Here go all the control functions*/

//TODO
/*define this constants if we need predefined shifting functions
#define L_PRECISION 1
#define  R_PRECISION 1 */
//#define BITSHIFT(num) ( (SHIFT) > 0 ? ( (num) >> (SHIFT) ) : ( (num) << -1*(SHIFT) ) )

#define RSHIFT(num,shift) ( (num) >> (shift) )
#define LSHIFT(num,shift) ( (num) << (shift) )
/*Same but more readable*/
#define INCREASE_SHIFT(num,shift) ( (num) << (shift) )
#define DECREASE_SHIFT(num,shift) ( (num) >> (shift) )

#define SENSOR_PRECISION 10


#endif
