#ifndef TYPES_H
#define TYPES_H
//#include <stdint.h>


typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS } Loglevel;
typedef enum {LOG_ONLY_IN_SAFE_MODE, MODE_ILLIGAL, MODE_CHANGE_ONLY_VIA_SAFE,
  	MODE_CHANGE_ONLY_IF_ZERO_RPM, MODE_ALREADY_SET, CONTROL_DISABLED_IN_THIS_MODE ,
	JS_LIFT_NOT_ZERO, SENSOR_LOG_FULL} Error;  /* Error Messages */

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef unsigned char     uint8_t;
typedef unsigned short int    uint16_t;
typedef unsigned int      uint32_t;

typedef union {
  uint32_t as_uint32_t;
  uint16_t as_uint16_t;
	int16_t as_int16_t;
	int8_t as_int8_t;
	uint8_t as_uint8_t;
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
#define NOT_IMPORTANT '~'

/************
 *  PC->QR  *
 ************/

/* control messages*/
#define MODE_CHANGE 'M'
#define JS_ROLL  'R'
#define JS_PITCH 'P'
#define JS_YAW   'Y'
#define JS_LIFT  'T'


/* Adjust parameters*/
#define ADJUST   'A'
// flight parameters
#define ROLL_LEFT 	'v'
#define ROLL_RIGHT 	'h'
#define PITCH_DOWN 	't'
#define PITCH_UP 	'g'
#define YAW_LEFT 	'q'
#define YAW_RIGHT 	'w'
#define LIFT_UP 	'a'
#define LIFT_DOWN 	'z'
// control parameters
#define P_YAW_UP 	'u'
#define P_YAW_DOWN 	'j'
#define P_ROLL_UP 	'i'
#define P_ROLL_DOWN 'k'
#define P_PITCH_UP 	'o'
#define P_PITCH_DOWN 'l'


/* SPECIAL REQUESTS */
#define SPECIAL_REQUEST 'S'  //= control value
/* Possible paramaters*/
#define ESCAPE 27 // TODO send this if pc-panic-button is pressed (if pc quits)
#define RESET_MOTORS 'r'
#define ASK_MOTOR_RPM 'm'
#define ASK_FILTER_PARAM 'f'
#define RESET_SENSOR_LOG 's'
#define ASK_SENSOR_LOG 'L'


/************
 *  QR->PC  *
 ************/


#define TERMINAL_MSG_START 'S'
#define TERMINAL_MSG_PART 'T'
#define TERMINAL_MSG_FINISH 'F'

#define ERROR_MSG 'E'
#define SENSOR_LOG_FULL 'O' //TODO implement
#define LOG_MSG_PART 'L'
#define LOG_MSG_NEW_LINE '\n'

/* Util functions */
bool is_valid_mode(Mode mode) {
	return mode >= SAFE && mode <= FULL_CONTROL;
}

PacketData swap_byte_order(PacketData p) {
	PacketData p2;
	p2.as_bytes[0] = p.as_bytes[1];
	p2.as_bytes[1] = p.as_bytes[0];
	return p2;
}

#endif /* TYPES_H */
