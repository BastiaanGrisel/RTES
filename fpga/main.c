/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>

#include "x32.h"
#include "checksum.h"
#include "logging.h"
#include "motors.h"
#include "control.h"

/* define some peripheral short hands
 */
#define X32_leds		peripherals[PERIPHERAL_LEDS]
#define X32_buttons		peripherals[PERIPHERAL_BUTTONS]
#define X32_ms_clock		peripherals[PERIPHERAL_MS_CLOCK]
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

#define OFFSET_STEP 10
#define TIMEOUT 500 //ms after which - if not receiving packets - the QR goes to panic mode
#define PANIC_RPM 100

/* Define global variables
 */
bool DEBUG = true;
int32_t  X32_ms_last_packet = -1; //ms of the last received packet. Set to -1 to avoid going panic before receiving the first msg
int32_t  time_at_last_led_switch = 0;
int32_t  packet_counter = 0, packet_lost_counter = 0;
int32_t  R=0, P=0, Y=0, T=0;
int missed_packet_counter;

bool is_calibrated = false;

/* filter parameters*/
int		Ybias = 400;
int 	filtered_dY = 0; //
int 	Y_BIAS_UPDATE = 10; // update bias each sample with a fraction of 1/2^13
int 	Y_FILTER = 3; // simple filter that updates 1/2^Y_filter
int 	P_yaw=12; // P = 2^4     Y_TO_ENGINE_SCALE
int 	Y_stabilize;
int 	dY;
 /*Roll parameters*/
int		R_FILTER = 3;
int		R_BIAS_UPDATE = 14;
int		R_ANGLE = 5;
int		R_ACC_RATIO = 2000;
int		R_ACC_BIAS = 0;  /*set this in CALIBRATION mode*/
int		C1_R = 7;
int		C2_R = 16;
int		P1_R = 6;
int		P2_R = 5;

int		dR = 0; // init (not very important)
int		Rangle = 0; // init to zero
int		Rbias = 0;// bitshift_l(calibratedRgyro,R_BIAS_UPDATE);
int		filtered_dR = 0;
int		R_stabilize = 0;


int32_t	s0, s1, s2, s3, s4, s5;
int32_t s_bias[6];

Fifo	pc_msg_q;

Mode	mode = SAFE;
int32_t panic_start_time = 0;
Loglevel log_level = SENSORS;

unsigned int sensor_log[LOG_SIZE][7];



void update_nexys_display(){
	nexys_display = packet_counter << 8 + packet_lost_counter;
	///nexys_display = [debugValue];
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
		s_bias[0] = s0;
		s_bias[1] = s1;
		s_bias[2] = s2;
		s_bias[3] = s3;
		s_bias[4] = s4;
		s_bias[5] = s5;

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

	if(new_mode == FULL_CONTROL) {
		R_ACC_BIAS = s_bias[0];
		Rbias = s_bias[3];
	}

	if(new_mode == FULL_CONTROL && !is_calibrated)
		return false;

	// If everything is OK, change the mode
	mode = new_mode;
	panic_start_time = X32_ms_clock;
	reset_motors();

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
			P_yaw++;
			break;
		case P_YAW_DOWN:
			P_yaw--;
			break;
		case P_ROLL_UP:
			P1_R++;
			P2_R++;
			break;
		case P_ROLL_DOWN:
			P1_R--;
			P2_R--;
			break;
		case P_PITCH_UP:
			break;
		case P_PITCH_DOWN:
			break;
		default:
			break;
	}
}

void special_request(char request){

	switch(request){

		case ESCAPE:
			set_mode(PANIC);
			break;
		case ASK_MOTOR_RPM:
			sprintf(message, "offset = [%i,%i,%i,%i], rpm = [%i,%i,%i,%i], rpyt = [%i,%i,%i,%i] T/4 = %i S = [%i,%i,%i,%i,%i,%i ]",get_motor_offset(0),get_motor_offset(1),get_motor_offset(2),get_motor_offset(3),get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3),R,P,Y,T,T/4,s_bias[0],s_bias[1],s_bias[2],s_bias[3],s_bias[4],s_bias[5]);
			send_term_message(message);
			break;
		case ASK_SENSOR_BIAS:
			sprintf(message, "Sensor bias = [%i,%i,%i,%i,%i,%i]",s_bias[0],s_bias[1],s_bias[2],s_bias[3],s_bias[4],s_bias[5]);
			send_term_message(message);
			break;
		case ASK_FILTER_PARAM:
			sprintf(message, "Y_stabilize = %i,  Ybias = %i, filtered_dY = %i\n R_stabilize = %i,", Y_stabilize, (Ybias >> Y_BIAS_UPDATE), (filtered_dY >> Y_BIAS_UPDATE), R_stabilize);
			send_term_message(message);
			break;
		case RESET_SENSOR_LOG:
			clear_log();
			X32_leds = X32_leds & 0x7F; // 01111111 = disable led 7
			break;
		case ASK_SENSOR_LOG:
			if(mode==SAFE) send_logs(sensor_log);
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
	X32_ms_last_packet= X32_ms_clock; //update the time the last packet was received

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

/* Two bitshift util functions. They interpre negative shift as a shift in the other direction.
 * Author: Bastiaan
 */
int32_t bitshift_r(int32_t value, int32_t shift) {
	return shift >= 0 ? value >> shift : value << -1 * shift;
}

int32_t bitshift_l(int32_t value, int32_t shift) {
	return bitshift_r(value, -1 * shift);
}

void record_bias(int32_t s_bias[6], int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5) {

	int32_t ratio = 10;

	s_bias[0]  += -1 * (s_bias[0] >> ratio) + s0;
	s_bias[1]  += -1 * (s_bias[1] >> ratio) + s1;
	s_bias[2]  += -1 * (s_bias[2] >> ratio) + s2;
	s_bias[3]  += -1 * (s_bias[3] >> ratio) + s3;
	s_bias[4]  += -1 * (s_bias[4] >> ratio) + s4;
	s_bias[5]  += -1 * (s_bias[5] >> ratio) + s5;
}

int32_t min(int32_t one, int32_t two) {
	return	 (one < two) ? one : two;
}

int32_t max(int32_t one, int32_t two) {
	return (one > two) ? one : two;
}

/* ISR when new sensor readings are read from the QR
 */
void isr_qr_link(void)
{
	/*
/* get sensor and timestamp values */
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;

	// Add new sensor values to array
	log_sensors(sensor_log, X32_QR_timestamp/50, s0, s1, s2, s3, s4, s5);

	/*YAW_CALCULATIONS*/
	//  scale dY up with Y_BIAS_UPDATE
	dY 		= (s5 << Y_BIAS_UPDATE) - Ybias;
	// update Ybias with 1/2^Y_BIAS_UPDATE each sample
	Ybias   	+= -1 * (Ybias >> Y_BIAS_UPDATE) + s5;
	// filter dY
	filtered_dY 	+= -1 * (filtered_dY >> Y_FILTER) + (dY >> Y_BIAS_UPDATE);
	// calculate stabilisation value
	Y_stabilize 	= Y+ bitshift_r(0 - filtered_dY, Y_BIAS_UPDATE - P_yaw);

	/*ROLL_CALCULATIONS*/
/*
//     substract bias and scale R:
    dR = bitshift_l(s3,R_BIAS_UPDATE)-Rbias;
//   filter
    filtered_dR+= - bitshift_l(filtered_dR,-R_FILTER) + bitshift_l(dR,-R_FILTER);
//     integrate for the angle and add something to react agianst
//     rounding error
    Rangle += bitshift_l(filtered_dR+bitshift_l(1,-R_BIAS_UPDATE+R_ANGLE-1),-R_BIAS_UPDATE+R_ANGLE);
    // kalman
	Rangle += - bitshift_l(Rangle-(s0-R_ACC_BIAS)*R_ACC_RATIO+bitshift_l(1,C1_R-1),-C1_R);
//		update bias
    Rbias += bitshift_l(Rangle-(s0-R_ACC_BIAS)*R_ACC_RATIO+ bitshift_l(1,C2_R-1),-C2_R);
//     calculate stabilization
    R_stabilize = R + bitshift_l(0-Rangle,-1*(R_BIAS_UPDATE - P1_R)) - bitshift_l(filtered_dR,-1*(R_BIAS_UPDATE - P2_R));

*/
	switch(mode) {
		case CALIBRATE:
			record_bias(s_bias, s0, s1, s2, s3, s4, s5);
			break;
		case MANUAL:
			// Calculate motor RPM
			set_motor_rpm(
				max(T/4, get_motor_offset(0) + T  +P+Y),
				max(T/4, get_motor_offset(1) + T-R  -Y),
				max(T/4, get_motor_offset(2) + T  -P+Y),
				max(T/4, get_motor_offset(3) + T+R  -Y));
			break;
		case YAW_CONTROL:
			// Calculate motor RPM
			set_motor_rpm(
				max(T/4, get_motor_offset(0) + T  +P+Y_stabilize),
				max(T/4, get_motor_offset(1) + T-R  -Y_stabilize),
				max(T/4, get_motor_offset(2) + T  -P+Y_stabilize),
				max(T/4, get_motor_offset(3) + T+R  -Y_stabilize));
			break;
		/*case FULL_CONTROL:
			// Calculate motor RPM
			set_motor_rpm(
				get_motor_offset(0) + T  +P+Y_stabilize,
				get_motor_offset(1) + T-R_stabilize  -Y_stabilize,
				get_motor_offset(2) + T  -P+Y_stabilize,
				get_motor_offset(3) + T+R_stabilize  -Y_stabilize);
			break;*/
		case PANIC:
			nexys_display = 0xc1a0;

			if(X32_ms_clock - panic_start_time < 2000) {
				set_motor_rpm(PANIC_RPM,PANIC_RPM,PANIC_RPM,PANIC_RPM);
			} else {
				reset_motors();
				R = P = Y = T = 0;
				set_mode(SAFE);
			}
			break;
	}
}

/* Make the throttle scale non-linear
 * 0-63   = 0-600
 * 64-255 = 600-800
 */
int16_t scale_throttle(uint8_t throttle) {
	if(throttle < 63) {
		return throttle * 10;
	} else {
		return throttle - 64 + 630;
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

	if(mode < MANUAL && !(control == MODE_CHANGE || control == ADJUST || control == SPECIAL_REQUEST)){
		sprintf(message, "[%c %i] Change mode to operate the QR!\n", control, data.as_char);
		send_term_message(message);
		return;
	}

	switch(control){
		case MODE_CHANGE:
			set_mode(data.as_char);
			break;
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


	fifo_init(&pc_msg_q);

  	if(DEBUG) init_array(sensor_log);

	/* Prepare Interrupts */

	// FPGA -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_XUFO, &isr_qr_link);
    	SET_INTERRUPT_PRIORITY(INTERRUPT_XUFO, 21);

	// PC -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_RX, &isr_rs232_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_RX, 20);
	while (X32_rs232_char) c = X32_rs232_data; // empty the buffer

	/* Enable Interrupts */

    	ENABLE_INTERRUPT(INTERRUPT_XUFO);
        ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX);

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
void check_alive_connection() {
	if(X32_ms_last_packet == -1) return; //does not perform the check untill a new message arrives

	// If a packet has not been received within the TIMEOUT interval, go to panic mode
	if((X32_ms_clock - X32_ms_last_packet > TIMEOUT) && mode >= MANUAL) {
		set_mode(PANIC);
		sprintf(message, "X32_ms_clock:%i, X32_ms_last_packet:%i, diff:%i, TIMEOUT:%i!\n", X32_ms_clock, X32_ms_last_packet, X32_ms_clock - X32_ms_last_packet, TIMEOUT);
		send_term_message(message);
	}
}

int32_t main(void)
{
	setup();

	// Main loop
	while (1) {
		// Pings from the PC
	   	check_alive_connection();

		// Turn on the LED corresponding to the mode and don't change led 6 and 7
		X32_leds = ((flicker_slow()?1:0) << mode) | (X32_leds & 0xC0);

		// Process messages
        	DISABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Disable incoming messages while working with the message queue
					check_for_new_packets(&pc_msg_q, &packet_received, &lost_packet);
		ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Re-enable messages from the PC after processing them
	}

	quit();

	return 0;
}
