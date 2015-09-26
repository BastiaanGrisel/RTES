#ifndef TYPES_H
#define TYPES_H

typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS } Loglevel;

#define LEFT_CHAR 'v'
#define RIGHT_CHAR 'h'
#define UP_CHAR 't'
#define DOWN_CHAR 'g'

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;


#endif /* TYPES_H */

