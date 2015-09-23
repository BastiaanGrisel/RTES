#ifndef TYPES_H
#define TYPES_H

typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS } Loglevel;

#define LEFT_CHAR 'v'
#define RIGHT_CHAR 'h'
#define UP_CHAR 't'
#define DOWN_CHAR 'g'

#endif /* TYPES_H */

