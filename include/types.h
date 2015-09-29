#ifndef TYPES_H
#define TYPES_H

typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS } Loglevel;

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;

typedef union {
	int16_t as_int16_t;
	int8_t as_int8_t;
	char as_char;
	char as_bytes[2];		
} PacketData;

/* Convert a character to packetdata to be sent over the serial connection
 */
PacketData ch2pd(char in) {
	PacketData p;
	p.as_bytes[0] = in;
	return p;
}

#define LEFT_CHAR 'v'
#define RIGHT_CHAR 'h'
#define UP_CHAR 't'
#define DOWN_CHAR 'g'

#endif /* TYPES_H */

