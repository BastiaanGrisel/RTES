#ifndef TYPES_H
#define TYPES_H

typedef enum { false, true } bool;
typedef enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } Mode;
typedef enum { NONE, SENSORS,YAW,FULL } Loglevel;
typedef enum {  LOG_ONLY_IN_SAFE_MODE,
                MODE_ILLIGAL,
                MODE_CHANGE_ONLY_VIA_SAFE,
  	            MODE_CHANGE_ONLY_IF_ZERO_RPM,
                MODE_ALREADY_SET,
                CONTROL_DISABLED_IN_THIS_MODE,
	              JS_LIFT_NOT_ZERO,
                SENSOR_LOG_FULL,
                FIRST_CALIBRATE,
                DIVISION_BY_ZERO_HAPPEND,
                OVERFLOW_HAPPENED } Error;  /* Error Messages */

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef unsigned char     uint8_t;
typedef unsigned short int    uint16_t;
typedef unsigned int      uint32_t;

typedef union {
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
#define P1_ROLL_UP 	'i'
#define P1_ROLL_DOWN 'k'
#define P2_ROLL_UP 	'o'
#define P2_ROLL_DOWN 'l'
#define P1_PITCH_UP 	'0'
#define P1_PITCH_DOWN '1'
#define P2_PITCH_UP 	'2'
#define P2_PITCH_DOWN '3'
#define Y_FILTER_UP '4'
#define Y_FILTER_DOWN '5'
#define R_FILTER_UP '6'
#define R_FILTER_DOWN '7'
#define P_FILTER_UP '8'
#define P_FILTER_DOWN '9'
#define JS_INFL_UP 'C'
#define JS_INFL_DOWN 'V'

/* SPECIAL REQUESTS */
#define SPECIAL_REQUEST 'S'  //= control value
/* Possible paramaters*/
#define ESCAPE 27
#define RESET_MOTORS 'r'
#define ASK_MOTOR_RPM 'm'
#define ASK_FILTER_PARAM 'f'
#define ASK_FULL_CONTROL_PARAM 'g'
#define RESET_SENSOR_LOG 's'
#define ASK_SENSOR_LOG 'x'
#define ASK_SENSOR_BIAS 'b'

/************
 *  QR->PC  *
 ************/

/*Since these are codes used only by the main function (i.e. are not keys),
shouldn't be cardinal ordered? Like A,B,C,ecc */
#define TERMINAL_MSG_START '0'
#define TERMINAL_MSG_PART '1'
#define TERMINAL_MSG_FINISH '2'

#define ERROR_MSG '3'
#define FB_MSG '4'
#define FB_MSG_END  '5'
#define SENSOR_LOG_FULL '6'
#define LOG_MSG_PART '7'
#define LOG_MSG_NEW_LINE '\n'


#define CURRENT_MODE 'm'
#define TIMESTAMP 't'
#define RPM0 '8'
#define RPM1 '9'
#define RPM2 'A'
#define RPM3 'B'
#define MY_STAB 'C'
#define MP_STAB 'D'
#define MR_STAB 'E'
#define MR_ANGLE 'F'
#define MP_ANGLE 'G'
#define SENS_0 'H'
#define SENS_1 'I'
#define SENS_2 'L'
#define SENS_3 'M'
#define SENS_4 'N'
#define SENS_5 'O'

#define LOG_EVENT 'P'
#define LOG_EV_NEW_LINE 'Q'

#define P_YAW 'R'
#define P1_ROLL 'S'
#define P2_ROLL 'T'
#define P1_PITCH 'U'
#define P2_PITCH 'V'
#define FILTER_R 'W'
#define FILTER_P 'X'
#define FILTER_Y 'Y'
#define JS_INFL 'Z'

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

char* mode_to_string(Mode m) {
	switch(m) {
		case SAFE: return "Safe";
		case PANIC: return "Panic";
		case MANUAL: return "Manual";
		case CALIBRATE: return "Calibration";
		case YAW_CONTROL: return "Yaw Control";
		case FULL_CONTROL: return "Full Control";
		default: return "N/A";
	}
}

#endif /* TYPES_H */
