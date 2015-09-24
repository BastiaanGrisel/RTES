#ifndef TYPES_H
#define TYPES_H

typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS } Loglevel;

#define ROLL_LEFT 'v'
#define ROLL_RIGHT 'h'
#define PITCH_DOWN 't'
#define PITCH_UP 'g'
#define YAW_LEFT 'q'
#define YAW_RIGHT 'w'
#define LIFT_UP 'a'
#define LIFT_DOWN 'z'

#define ASK_MOTOR_RPM 'm'
#define ASK_FILTER_PARAM 'f'
#define RESET_SENSOR_LOG 's'
#define SENSOR_LOG_FULL 'S'
#define P_YAW_UP 'u'
#define P_YAW_DOWN 'j'
#define P_ROLL_UP 'i'
#define P_ROLL_DOWN 'k'
#define P_PITCH_UP 'o'
#define P_PITCH_DOWN 'l'
/*
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define 
#define */


#endif /* TYPES_H */

