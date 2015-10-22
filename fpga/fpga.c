/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>

//#define DOUBLESENSORS

#include "x32.h"
#include "checksum.h"
#include "logging.h"
#include "motors.h"
#include "control.h"
#include "filterYAW.h"
#include "filterROLL.h"
#include "filterPITCH.h"

/* define some peripheral short hands
 */
#define X32_leds		peripherals[PERIPHERAL_LEDS]
#define X32_buttons		peripherals[PERIPHERAL_BUTTONS]
#define X32_ms_clock	peripherals[PERIPHERAL_MS_CLOCK]
#define X32_us_clock 	(X32_ms_clock *1000)

#define X32_QR_s0 		peripherals[PERIPHERAL_XUFO_S0]
#define X32_QR_s1 		peripherals[PERIPHERAL_XUFO_S1]
#define X32_QR_s2 		peripherals[PERIPHERAL_XUFO_S2]
#define X32_QR_s3 		peripherals[PERIPHERAL_XUFO_S3]
#define X32_QR_s4 		peripherals[PERIPHERAL_XUFO_S4]
#define X32_QR_s5 		peripherals[PERIPHERAL_XUFO_S5]
#define nexys_display peripherals[0x05] //when needed
#define X32_QR_timestamp 	peripherals[PERIPHERAL_XUFO_TIMESTAMP]

#define X32_rs232_data		peripherals[PERIPHERAL_PRIMARY_DATA]
#define X32_rs232_stat		peripherals[PERIPHERAL_PRIMARY_STATUS]
#define X32_rs232_char		(X32_rs232_stat & 0x02)
#define X32_rs232_write		(X32_rs232_stat & 0x01)

#define OFFSET_STEP 5
#define TIMEOUT 500 //ms after which - if not receiving packets - the QR goes to panic mode
#define PANIC_RPM 400

#define DEBUG 0
#define TIMEANALYSIS 1


/* Define global variables
 */

int32_t  X32_ms_last_packet = -1; //ms of the last received packet. Set to -1 to avoid going panic before receiving the first msg
int32_t  time_last_sensor_input = 0;
int32_t  packet_counter, packet_lost_counter = 0;
int32_t  R=0, P=0, Y=0, T=0, Tmin=0;
int32_t  R_amp = 4, P_amp = 4, Y_amp = 4; //curresponging amplified variables for MANUAL
int missed_packet_counter;

bool is_calibrated = false;

int32_t 	isr_qr_counter = 0;

int32_t	s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
#ifdef DOUBLESENSORS
int32_t	ds0 = 0, ds1 = 0, ds2 = 0, ds3 = 0, ds4 = 0, ds5 = 0;
#endif
int32_t s_bias[6] = {0};
int32_t isr_counter = 0;

Fifo	pc_msg_q;

Mode mode = SAFE;
int32_t mode_start_time = 0;
//Loglevel log_level = SENSORS;

bool always_log = true;
bool log_ev_completed = false;
bool log_data_completed = false;
//LOGGING ARRAYS
int16_t tm_array[LOG_SIZE][3];       //Timestamp and Mode
int16_t sbias_array[6]; 					 //sbias
int16_t sensor_array[LOG_SIZE][10];  //sensors + actuators
int16_t control_array[LOG_SIZE][5]; //control chain
int16_t event_array[LOG_EVENT][4]; //events: change mode, keys and js


void update_nexys_display(){
	nexys_display = packet_counter << 8 + packet_lost_counter;
}

void lost_packet()
{
	packet_lost_counter++;
	sprintf(message,"Lost packets: %d\n",packet_lost_counter);
	send_term_message(message);
	update_nexys_display();
}

/*
 * Changes the mode to either: SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL or FULL_CONTROL.
 * Motor RPM needs to be zero to change mode except when changing to SAFE and PANIC
 * Returns a boolean indicating whether the mode change was succesful or not.
 * Author: Alessio
 */
bool set_mode(Mode new_mode) {
	int32_t i;

	if(!is_valid_mode(new_mode)) {
		send_err_message(MODE_ILLIGAL);
		return false;
	}

	if(mode == new_mode) {
		send_err_message(MODE_ALREADY_SET);
		return false;
	}

	if(new_mode == CALIBRATE) {
		s_bias[0] = s0 << SENSOR_PRECISION;
		s_bias[1] = s1 << SENSOR_PRECISION;
		s_bias[2] = s2 << SENSOR_PRECISION;
		s_bias[3] = s3 << SENSOR_PRECISION;
		s_bias[4] = s4 << SENSOR_PRECISION;
		s_bias[5] = s5 << SENSOR_PRECISION;

		is_calibrated = true;
	}

	if(new_mode >= MANUAL) {
		// Make sure that a change to an operational mode can only be done via SAFE
		if(mode >= MANUAL){
			send_err_message(MODE_CHANGE_ONLY_VIA_SAFE);
			return false;
		}

		// If at least one of the motor's RPM is not zero, return false
		if(!motors_have_zero_rpm()) {
			send_err_message(MODE_CHANGE_ONLY_IF_ZERO_RPM);
			return false;
		}
	}



	if((new_mode == FULL_CONTROL || new_mode == YAW_CONTROL) && !is_calibrated){
		send_err_message(FIRST_CALIBRATE);
		return false;
	}

	if(mode == CALIBRATE) {
		log_sbias(sbias_array,s_bias); 	//logging s_bias only at end of calibration mode
		//maybe we can make this a function call
		Ybias = s_bias[5] << (Y_BIAS_UPDATE-SENSOR_PRECISION);
		R_ACC_BIAS = DECREASE_SHIFT(s_bias[1]*R_ACC_RATIO,SENSOR_PRECISION);
		Rbias = INCREASE_SHIFT(s_bias[3],C2_R_BIAS_UPDATE-SENSOR_PRECISION);
		P_ACC_BIAS = DECREASE_SHIFT(s_bias[0]*P_ACC_RATIO,SENSOR_PRECISION);
		Pbias = INCREASE_SHIFT(s_bias[4],C2_P_BIAS_UPDATE-SENSOR_PRECISION);
	}

	// If everything is OK, change the mode
	reset_motors();
	mode = new_mode;
	mode_start_time = X32_ms_clock;
	log_event(event_array,X32_US_CLOCK,MODE_CHANGE,mode);

	send_int_message(CURRENT_MODE,mode);
	sprintf(message, "Succesfully changed to mode: >%i< ", new_mode);
	send_term_message(message);
	return true;
}

/* trim the QR
 * always use this offset when sending the motor commands
 * Author: Henko
 */
void trim(char c){
	switch(c){
		case 'a': // throttle up
			add_motor_offset(+OFFSET_STEP, +OFFSET_STEP, +OFFSET_STEP, +OFFSET_STEP);
			break;
		case 'z': // throttle down
			add_motor_offset(-OFFSET_STEP, -OFFSET_STEP, -OFFSET_STEP, -OFFSET_STEP);
			break;
		case ROLL_LEFT: // roll left
			add_motor_offset(0, +OFFSET_STEP, 0, -OFFSET_STEP);
			break;
		case ROLL_RIGHT: // roll right
			add_motor_offset(0, -OFFSET_STEP, 0, +OFFSET_STEP);
			break;
		case PITCH_DOWN: // pitch down
			add_motor_offset(-OFFSET_STEP, 0, +OFFSET_STEP, 0);
			break;
		case PITCH_UP: // pitch up
			add_motor_offset(+OFFSET_STEP, 0, -OFFSET_STEP, 0);
			break;
		case YAW_LEFT: // yaw left
			add_motor_offset(-OFFSET_STEP, +OFFSET_STEP, -OFFSET_STEP, +OFFSET_STEP);
			break;
		case YAW_RIGHT: // yaw right
			add_motor_offset(+OFFSET_STEP, -OFFSET_STEP, +OFFSET_STEP, -OFFSET_STEP);
			break;
		case P_YAW_UP:
			increase_P_yaw();
			break;
		case P_YAW_DOWN:
			decrease_P_yaw();
			break;
		case P1_ROLL_UP:
			P1_roll_minor++;
			break;
		case P1_ROLL_DOWN:
			P1_roll_minor--;
			break;
		case P2_ROLL_UP:
			P2_roll_minor++;
			break;
		case P2_ROLL_DOWN:
			P2_roll_minor--;
			break;
		case P1_PITCH_UP:
			P1_pitch_minor++;
			break;
		case P1_PITCH_DOWN:
			P1_pitch_minor--;
			break;
		case P2_PITCH_UP:
			P2_pitch_minor++;
			break;
		case P2_PITCH_DOWN:
			P2_pitch_minor--;
			break;
		case Y_FILTER_UP:
		   Y_filter++;
			 break;
		case Y_FILTER_DOWN:
		   Y_filter--;
			 break;
		case R_FILTER_UP:
			R_filter++;
			break;
		case R_FILTER_DOWN:
			R_filter--;
			break;
		case P_FILTER_UP:
			P_filter++;
			break;
		case P_FILTER_DOWN:
			P_filter--;
			break;
		case JS_INFL_R_UP:
			if(mode == MANUAL) R_amp++;
			if(mode > MANUAL) RJS_TO_ANGLE_RATIO+=20;
			break;
		case JS_INFL_R_DOWN:
			if(mode == MANUAL) R_amp--;
			if(mode > MANUAL) RJS_TO_ANGLE_RATIO-=20;
			break;
		case JS_INFL_P_UP:
			if(mode == MANUAL) P_amp++;
			if(mode > MANUAL) PJS_TO_ANGLE_RATIO+=20;
			break;
		case JS_INFL_P_DOWN:
			if(mode == MANUAL) P_amp--;
			if(mode > MANUAL) PJS_TO_ANGLE_RATIO-=20;
			break;
		case JS_INFL_Y_UP:
			if(mode == MANUAL) Y_amp++;
			if(mode > MANUAL) YJS_TO_ANGLE_RATIO++;
			break;
		case JS_INFL_Y_DOWN:
			if(mode == MANUAL) Y_amp--;
			if(mode > MANUAL) YJS_TO_ANGLE_RATIO--;
			break;
		default:
			break;
	}
}

void special_request(char request){

	switch(request){

		case ESCAPE:
			if(mode >= MANUAL) set_mode(PANIC);
			break;
		case ASK_MOTOR_RPM:
			sprintf(message, "offset = [%i,%i,%i,%i], rpm = [%i,%i,%i,%i], rpyt = [%i,%i,%i,%i] T/4 = %i",get_motor_offset(0),get_motor_offset(1),get_motor_offset(2),get_motor_offset(3),get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3),R,P,Y,T,T/4);
			send_term_message(message);
			break;
		case ASK_SENSOR_BIAS:
			sprintf(message, "Sensor bias = [%i,%i,%i,%i,%i,%i]",s_bias[0] >> SENSOR_PRECISION,s_bias[1] >> SENSOR_PRECISION,s_bias[2] >> SENSOR_PRECISION,s_bias[3] >> SENSOR_PRECISION,s_bias[4] >> SENSOR_PRECISION,s_bias[5] >> SENSOR_PRECISION);
			send_term_message(message);
			break;
		case ASK_FILTER_PARAM:
			getYawParams(message);
			send_term_message(message);
			break;
		case ASK_FULL_CONTROL_PARAM:
			sprintf(message, "dR = %i,  Rangle = %i, Rbias = %i, filtered_dR = %i, R_stabilize = %i", dR>>C2_R_BIAS_UPDATE, DECREASE_SHIFT(R_angle,C2_R_BIAS_UPDATE-R_ANGLE), Rbias, filtered_dR, R_stabilize);
			send_term_message(message);
			break;
		case RESET_SENSOR_LOG:
			clear_log();
			send_term_message("Resetted logs. New log will start now.");
			X32_leds = X32_leds & 0x7F; // 01111111 = disable led 7
			break;
		case ASK_SENSOR_LOG:
			if(mode==SAFE) send_logs(tm_array,sbias_array,sensor_array,control_array), send_logs_event(event_array);
		   	else send_err_message(LOG_ONLY_IN_SAFE_MODE);

			break;
		case RESET_MOTORS: //reset
			reset_motors();
			break;
	}
}

/*
 * Process interrupts from serial connection
 * Author: Bastiaan Grisel
 */
void isr_rs232_rx(void)
{
	char c;
	X32_ms_last_packet = X32_ms_clock; //update the time the last packet was received

	packet_counter++;
	update_nexys_display();

	/* handle all bytes, note that the processor will sometimes generate
		* an interrupt while there is no byte available, make sure the handler
		* checks the state of the com channel before fetching a character from
		* the buffer. Also it is recommended to use a while loop to handle all
		* available characters.
		*/
	while (X32_rs232_char) {
		c = X32_rs232_data;

		// Add the message to the message queue
		fifo_put(&pc_msg_q, c);
	}
}

void record_bias(int32_t s_bias[6], int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5) {
	s_bias[0]  -= DECREASE_SHIFT(s_bias[0],SENSOR_PRECISION) - s0;
	s_bias[1]  -= DECREASE_SHIFT(s_bias[1],SENSOR_PRECISION) - s1;
	s_bias[2]  -= DECREASE_SHIFT(s_bias[2],SENSOR_PRECISION) - s2;
	s_bias[3]  -= DECREASE_SHIFT(s_bias[3],SENSOR_PRECISION) - s3;
	s_bias[4]  -= DECREASE_SHIFT(s_bias[4],SENSOR_PRECISION) - s4;
	s_bias[5]  -= DECREASE_SHIFT(s_bias[5],SENSOR_PRECISION) - s5;
}

#define min(one, two) ((one < two) ? one : two)

#define max(one, two) ((one > two) ? one : two)

/* ISR when new sensor readings are read from the QR
*/
void isr_qr_link(void)
{
	bool log_cond;
	int timeTime, timeRead, timeLog, timeYaw, timeFull,timeAct, timestart = X32_US_CLOCK;
	time_last_sensor_input = X32_ms_clock;
	if(TIMEANALYSIS) timeTime = X32_US_CLOCK;
/* get sensor and timestamp values */


	#ifdef DOUBLESENSORS
		if(isr_qr_counter==1) {
			ds0 =s0+ X32_QR_s0; ds1 =s1+ X32_QR_s1; ds2 =s2+ X32_QR_s2;
			ds3 =s3+ X32_QR_s3; ds4 =s4+ X32_QR_s4; //ds5 =s5+ X32_QR_s5;
		} else {
			s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
			s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;
		}
	#else
		s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
		s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;
	#endif /*DOUBLESENSORS*/


	if(TIMEANALYSIS) timeRead = X32_US_CLOCK;

	Y_stabilize = yawControl(s5,Y);

	if(TIMEANALYSIS) timeYaw = X32_US_CLOCK;
	//QR THREE IS FLIPPED!!
	if(isr_qr_counter++ == 1) {
		isr_qr_counter = 0;


		if(mode >= YAW_CONTROL) ENABLE_INTERRUPT(INTERRUPT_OVERFLOW);

		#ifndef DOUBLESENSORS
			R_stabilize = rollControl(s3,s1,R);
			P_stabilize = pitchControl(s4,s0,P);
		#endif
		#ifdef DOUBLESENSORS
			R_stabilize = rollControl(ds3,ds1,R);
			P_stabilize = pitchControl(ds4,ds0,P);
		#endif


		if(TIMEANALYSIS) timeFull = X32_US_CLOCK;

		switch(mode) {
			case CALIBRATE:
				record_bias(s_bias, s0, s1, s2, s3, s4, s5);
				//R_angle = P_angle = 0;
				break;
			case MANUAL:
				set_motor_rpm(
					max(Tmin, get_motor_offset(0) + T  + (P_amp*P) >> 2 + (Y_amp*Y) >> 2),
					max(Tmin, get_motor_offset(1) + T	 - (R_amp*R) >> 2 - (Y_amp*Y) >> 2),
					max(Tmin, get_motor_offset(2) + T  - (P_amp*P) >> 2 + (Y_amp*Y) >> 2),
					max(Tmin, get_motor_offset(3) + T  + (R_amp*R) >> 2 - (Y_amp*Y) >> 2));
				break;
			case YAW_CONTROL:
				// Calculate motor RPM
				set_motor_rpm(
					max(Tmin, get_motor_offset(0) + T  +P+Y_stabilize),
					max(Tmin, get_motor_offset(1) + T-R  -Y_stabilize),
					max(Tmin, get_motor_offset(2) + T  -P+Y_stabilize),
					max(Tmin, get_motor_offset(3) + T+R  -Y_stabilize));
				break;
			case FULL_CONTROL:
				// Calculate motor RPM
				set_motor_rpm(
					max(Tmin, get_motor_offset(0) + T   +P_stabilize	+Y_stabilize),
					max(Tmin, get_motor_offset(1) + T		-R_stabilize  -Y_stabilize),
					max(Tmin, get_motor_offset(2) + T   -P_stabilize	+Y_stabilize),
					max(Tmin, get_motor_offset(3) + T	  +R_stabilize  -Y_stabilize));
				break;
			case PANIC:
				nexys_display = 0xc1a0;

				if(X32_ms_clock - mode_start_time < 2000) {
					set_motor_rpm(
							min(PANIC_RPM, get_motor_rpm(0)),
							min(PANIC_RPM, get_motor_rpm(1)),
							min(PANIC_RPM, get_motor_rpm(2)),
							min(PANIC_RPM, get_motor_rpm(3)));
				} else {
					reset_motors();
					set_mode(SAFE);
				}
				break;
		}

		if(mode >= YAW_CONTROL) DISABLE_INTERRUPT(INTERRUPT_OVERFLOW);

		if(TIMEANALYSIS) timeAct = X32_US_CLOCK;

  {
		log_cond = (mode >= MANUAL && (always_log || !log_data_completed));
			if(log_cond) {  //if always_log is false, it will perform only one logging
					log_tm(tm_array, X32_QR_timestamp,mode); 	// Logging timestamp and mode
					if(mode == CALIBRATE) log_sbias(sbias_array,s_bias); 	//logging s_bias only in calibration mode

					if(mode >= YAW_CONTROL) DISABLE_INTERRUPT(INTERRUPT_OVERFLOW);

					//in case of debug will log arbitrary values, so this function isn't called
					if(!DEBUG) log_sensors(sensor_array, s0, s1, s2, s3, s4, s5,get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
					log_control(mode, control_array, R_stabilize,P_stabilize,Y_stabilize,R_angle,P_angle); 	//logging control parameters
			}
	}

	}
	if(TIMEANALYSIS){
		timeLog = X32_US_CLOCK;
		if(isr_counter++ ==999){
			int timefinish = X32_US_CLOCK;
			int timeoffset = timeTime -timestart;
			isr_counter =1;
			sprintf(message, "\ntimeoffset = %4i, Read = %4i, \nYaw = %4i, Full = %4i, Act = %4i, Log = %4i, total = %4i",
				timeoffset,timeRead-timeTime-timeoffset,timeYaw-timeRead-timeoffset,
				timeFull-timeYaw-timeoffset, timeAct-timeFull-timeoffset,
				timeLog-timeAct-timeoffset,timefinish-timestart-timeoffset*7);
			send_term_message(message);
			/*sprintf(message,"sending a very long (for example: debug-) message = %4i", X32_US_CLOCK-timefinish);
			send_term_message(message);*/
		}
	}
}

/* Make the throttle scale non-linear
 * 0-45   = 0-450
 * 46-255 = 450-660
 */
int16_t scale_throttle(uint8_t throttle) {
	if(throttle < 45) {
		return throttle * 10;
	} else {
				return throttle - 45 + 450;
	}
}

/* Callback that gets executed when a packet has arrived
 * Author: Bastiaan
 */
void packet_received(char control, PacketData data) {
	//data = swap_byte_order(data);
	if(control == 0) return;

	//sprintf(message, "Packet Received: %c %c\n#", control, data.as_char);
	//send_term_message(message);

	if((mode < MANUAL ||mode == CALIBRATE) && !(control == MODE_CHANGE || control == ADJUST || control == SPECIAL_REQUEST)){
		sprintf(message, "[%c %i] Change mode to operate the QR!\n", control, data.as_char);
		send_term_message(message);
		return;
	}
	if(control == MODE_CHANGE){
		set_mode(data.as_char);
		// logging happens inside the set_mode
		return;
	}

		if(always_log || !log_data_completed)
	  			log_event(event_array,X32_US_CLOCK,control,data.as_int8_t); //logging all the events


	switch(control){
		case JS_ROLL:
			R = data.as_int8_t;
			break;
		case JS_PITCH:
			P = data.as_int8_t;
			break;
		case JS_YAW:
			Y = data.as_int8_t;
			break;
		case JS_LIFT:
			T = scale_throttle(data.as_int8_t);
			Tmin = T>>2;
			//sprintf(message, "Throttle: %i",T);
			//send_term_message(message);
			break;
		case ADJUST:
			trim(data.as_int8_t);
			break;
		case SPECIAL_REQUEST:
			special_request(data.as_char);
			break;
	}
}

/* Send an error message to the PC
 * Author: Bastiaan
 */
void div0_isr() {
	send_err_message(DIVISION_BY_ZERO_HAPPEND);
}

/* Send an error message to the PC
 * Author: Bastiaan
 */
void overflow_isr() {
	send_err_message(OVERFLOW_HAPPENED);
}

/* The startup routine.
 * Originally created by: Bastiaan
 */
void setup()
{
	int32_t c;
  char message[200];

	/* Initialize Variables */
	nexys_display = 0x00;
  missed_packet_counter = 0;
	R_angle = 0;
	P_angle = 0;

	always_log = true;
  log_ev_completed = false;
  log_data_completed = false;
	init_log_arrays(tm_array,sbias_array,sensor_array,control_array,event_array);

	fifo_init(&pc_msg_q);

  if(DEBUG) init_array_test(tm_array,sensor_array,control_array);

	/* Prepare Interrupts */

	// FPGA -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_XUFO, &isr_qr_link);
    	SET_INTERRUPT_PRIORITY(INTERRUPT_XUFO, 21);

	// PC -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_RX, &isr_rs232_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_RX, 20);
	while (X32_rs232_char) c = X32_rs232_data; // empty the buffer

	// Exception interrupts
	INTERRUPT_VECTOR(INTERRUPT_OVERFLOW) = &overflow_isr;
		INTERRUPT_PRIORITY(INTERRUPT_OVERFLOW) = 19;

	INTERRUPT_VECTOR(INTERRUPT_DIVISION_BY_ZERO) = &div0_isr;
		INTERRUPT_PRIORITY(INTERRUPT_DIVISION_BY_ZERO) = 19;

	/* Enable Interrupts */
  ENABLE_INTERRUPT(INTERRUPT_XUFO);
  ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX);
	ENABLE_INTERRUPT(INTERRUPT_DIVISION_BY_ZERO);

	/* Enable all interupts after init code */
	ENABLE_INTERRUPT(INTERRUPT_GLOBAL);
}

/* Function that is called when the program terminates
 * Originally created by: Bastiaan
 */
void quit()
{
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);
}

/* Function that is used to blink the LEDs.
 * Returns a boolean whode value depends on the time the function is called
 * Author: Bastiaan
 */
bool flicker_slow() { return (X32_ms_clock % 1000 < 200); }
bool flicker_fast() { return (X32_ms_clock % 100 < 20); }

/* checks if the QR is receiving packet in terms of ms defined by the TIMEOUT variable,
 * otherwise panic() is called.
 * Author: Alessio
 */
int check_alive_connection() {
	if(X32_ms_last_packet == -1) return 0; //does not perform the check untill a new message arrives

	// If a packet has not been received within the TIMEOUT interval, go to panic mode
	if((X32_ms_clock - X32_ms_last_packet > TIMEOUT)){
		if(mode >= MANUAL) {
			set_mode(PANIC);
			sprintf(message, "X32_ms_clock:%i, X32_ms_last_packet:%i, diff:%i, TIMEOUT:%i!\n", X32_ms_clock, X32_ms_last_packet, X32_ms_clock - X32_ms_last_packet, TIMEOUT);
			send_term_message(message);
		}
		return 0;
	}
	return 1;
}



/*Send real-time feedback from the QR.
Included: Timestamp mode sensors[6] RPM, control and signal proc chain values, telemetry.
Author: Alessio*/
void send_feedback()		//TODO: make this function parametric in order to put it in header file.
{
	int sendStart = X32_US_CLOCK;
	send_int_message(TIMESTAMP,X32_ms_clock);
	send_int_message(CURRENT_MODE,mode);

	send_int_message(SENS_0,s0 - (s_bias[0] >> SENSOR_PRECISION));
	send_int_message(SENS_1,s1 - (s_bias[1] >> SENSOR_PRECISION));
	send_int_message(SENS_2,s2 - (s_bias[2] >> SENSOR_PRECISION));
	send_int_message(SENS_3,s3 - (s_bias[3] >> SENSOR_PRECISION));
	send_int_message(SENS_4,s4 - (s_bias[4] >> SENSOR_PRECISION));
	send_int_message(SENS_5,s5 - (s_bias[5] >> SENSOR_PRECISION));

	send_int_message(RPM0,get_motor_rpm(0));
	send_int_message(RPM1,get_motor_rpm(1));
	send_int_message(RPM2,get_motor_rpm(2));
	send_int_message(RPM3,get_motor_rpm(3));

	send_int_message(MP_ANGLE,DECREASE_SHIFT(P_angle,C2_P_BIAS_UPDATE-P_ANGLE));
	send_int_message(MR_ANGLE,DECREASE_SHIFT(R_angle,C2_R_BIAS_UPDATE-R_ANGLE));

	if(mode >= YAW_CONTROL) {
		 send_int_message(MR_STAB,R_stabilize);
		 send_int_message(MP_STAB,P_stabilize);
		 send_int_message(MY_STAB,Y_stabilize);
 	}

	send_int_message(P_YAW, P_yaw);

  send_int_message(P1_ROLL, P1_roll_minor);
  send_int_message(P2_ROLL, P2_roll_minor);

  send_int_message(P1_PITCH, P1_pitch_minor);
  send_int_message(P2_PITCH, P2_pitch_minor);

  send_int_message(FILTER_R, R_filter);
  send_int_message(FILTER_P, P_filter);
  send_int_message(FILTER_Y, Y_filter);

	if(mode == MANUAL) {
		send_int_message(JS_INFL_R, R_amp);
		send_int_message(JS_INFL_P, P_amp);
		send_int_message(JS_INFL_Y, Y_amp);
	} else if(mode >= YAW_CONTROL) {
		send_int_message(JS_INFL_R, RJS_TO_ANGLE_RATIO);
		send_int_message(JS_INFL_P, PJS_TO_ANGLE_RATIO);
		send_int_message(JS_INFL_Y, YJS_TO_ANGLE_RATIO);
	} else {
		send_int_message(JS_INFL_R, 0);
		send_int_message(JS_INFL_P, 0);
		send_int_message(JS_INFL_Y, 0);
	}

  //send_int_message(JS_INFL, )

	if(DEBUG) sprintf(message, "time it takes to send all this feedback: %6i us",X32_US_CLOCK - sendStart);
	if(DEBUG) send_term_message(message);
}

int32_t main(void)
{
	int feedback_is_send=0;
	int PClinkisOK =0;
	setup();

	// Main loop
	while (1) {
		// Pings from the PC
	  PClinkisOK = check_alive_connection();
		if(TIMEANALYSIS) isr_qr_link();

		// Turn on the LED corresponding to the assignment
		X32_leds = (flicker_slow()?1:0) | (PClinkisOK*2) | ((X32_ms_clock-time_last_sensor_input<100)?4:0);

		// Process messages
  	DISABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Disable incoming messages while working with the message queue
		check_for_new_packets(&pc_msg_q, &packet_received, &lost_packet);
		ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Re-enable messages from the PC after processing them

		if(X32_ms_clock % 100 == 0) send_feedback();
	}

	quit();

	return 0;
}
