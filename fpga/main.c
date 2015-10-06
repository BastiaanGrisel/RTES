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
//#define LOG_SIZE 10
#define PANIC_RPM 100

/* Define global variables
 */

int32_t  X32_ms_last_packet = -1; //ms of the last received packet. Set to -1 to avoid going panic before receiving the first msg
int32_t  time_at_last_led_switch = 0;
int32_t  packet_counter = 0, packet_lost_counter = 0;
int32_t	 isr_qr_time = 0, isr_qr_counter = 0;
int32_t  R=0, P=0, Y=0, T=0;

/* filter parameters*/
int	Ybias = 400;
int 	filtered_dY = 0; //
int 	Y_BIAS_UPDATE = 10; // update bias each sample with a fraction of 1/2^13
int 	Y_FILTER = 3; // simple filter that updates 1/2^Y_filter
int 	P_yaw=12; // P = 2^4     Y_TO_ENGINE_SCALE
int 	Y_stabilize;
int dY;

int	s0, s1, s2, s3, s4, s5;
Fifo	pc_msg_q;

Mode	mode = SAFE;
int panicTimer = 0;
Loglevel log_level = SENSORS;

unsigned int sensor_log[LOG_SIZE][7];
int sensor_log_counter = 0;

char message[100];

/* function declarations TODO put this in header */
void panic(void);




void update_nexys_display(){
	nexys_display = packet_counter << 8 + packet_lost_counter;
	///nexys_display = [debugValue];
}

void lost_packet()
{
	packet_lost_counter++;
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

	// If everything is OK, change the mode
	mode = new_mode;
	panicTimer = 0;
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
			break;
		case P_ROLL_DOWN:
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
			sprintf(message, "offset = [%i%i%i%i]\nmotor RPM= [%i%i%i%i]\n",get_motor_offset(0),get_motor_offset(1),get_motor_offset(2),get_motor_offset(3),get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
			send_term_message(message);
			break;
		case ASK_FILTER_PARAM:
			sprintf(message, "dY = %i, Y_stabilize = %i,  Ybias = %i, filtered_dY = %i\n#", dY >> Y_BIAS_UPDATE, Y_stabilize, (Ybias >> Y_BIAS_UPDATE), (filtered_dY >> Y_BIAS_UPDATE));
			send_term_message(message);
			break;
		case RESET_SENSOR_LOG:
			sensor_log_counter = 0;
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
	isr_qr_time = X32_us_clock;
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


	isr_qr_time = X32_us_clock - isr_qr_time; //data to be logged
}


/* ISR when new sensor readings are read from the QR
 */
void isr_qr_link(void)
{
	isr_qr_time = X32_us_clock;

	/* get sensor and timestamp values */
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;

/*	if(sensor_log_counter < LOG_SIZE) {
		sensor_log[sensor_log_counter][0] = X32_QR_timestamp/50;
		sensor_log[sensor_log_counter][1] = s0;
		sensor_log[sensor_log_counter][2] = s1;
		sensor_log[sensor_log_counter][3] = s2;
		sensor_log[sensor_log_counter][4] = s3;
		sensor_log[sensor_log_counter][5] = s4;
		sensor_log[sensor_log_counter][6] = s5;
		sensor_log_counter++;
		if(sensor_log_counter == LOG_SIZE){
			send_err_message(SENSOR_LOG_FULL);
		}
	}*/

	/*YAW_CALCULATIONS*/
	dY 		= (s5 << Y_BIAS_UPDATE) - Ybias; 		// dY is now scaled up with Y_BIAS_UPDATE
	Ybias   	+= -1 * (Ybias >> Y_BIAS_UPDATE) + s5; 		// update Ybias with 1/2^Y_BIAS_UPDATE each sample
	filtered_dY 	+= -1 * (filtered_dY >> Y_FILTER) + (dY >> Y_BIAS_UPDATE); 	// filter dY
	//Y +=filtered_dY;						// integrate dY to get yaw (but if I remem correct then we need the rate not the yaw)
	if((Y_BIAS_UPDATE - P_yaw) >= 0) {
		Y_stabilize 	= (0 - filtered_dY) >> (Y_BIAS_UPDATE - P_yaw); // calculate error of yaw rate
	} else {
		Y_stabilize 	= (0 - filtered_dY) << (-Y_BIAS_UPDATE + P_yaw); // calculate error of yaw rate
	}

	switch(mode) {
		case MANUAL:
			// Calculate motor RPM
			set_motor_rpm(
				get_motor_offset(0) + T  +P+Y,
				get_motor_offset(1) + T-R  -Y,
				get_motor_offset(2) + T  -P+Y,
				get_motor_offset(3) + T+R  -Y);
			break;
		case YAW_CONTROL:
			// Calculate motor RPM
			set_motor_rpm(
				get_motor_offset(0) + T  +P+Y_stabilize,
				get_motor_offset(1) + T-R  -Y_stabilize,
				get_motor_offset(2) + T  -P+Y_stabilize,
				get_motor_offset(3) + T+R  -Y_stabilize);
			break;
		case PANIC:
			nexys_display = 0xc1a0;
			
			if(panicTimer++ < 3000){
				set_motor_rpm(PANIC_RPM,PANIC_RPM,PANIC_RPM,PANIC_RPM);
			} else {
				reset_motors();
				set_mode(SAFE);
			}
			break;
	}

	isr_qr_time = X32_us_clock - isr_qr_time; // why does this happen here and also at the end of the other ISR?
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
			if(data.as_int8_t >= 0)
				T = scale_throttle(data.as_int8_t);
			else
				T = scale_throttle(256 + data.as_int8_t);
			break;
		case ADJUST:
			trim(data.as_int8_t);
			break;
		case SPECIAL_REQUEST:
			special_request(data.as_char);
			break;
		default:
			break;
	}
}

/* The startup routine.
 * Originally created by: Bastiaan
 */
void setup()
{
	int32_t c;

	/* Initialize Variables */
	nexys_display = 0x00;
	isr_qr_counter = isr_qr_time = 0;

	fifo_init(&pc_msg_q);

  	//init_array(sensor_log);

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

/*Puts the FPGA to sleep performing an active waiting.
Author: Alessio*/
void X32_sleep(int32_t millisec) {
	long sleep_ms = X32_ms_clock +  (millisec);
	while(X32_ms_clock < sleep_ms);
	return;
}

/* checks if the QR is receiving packet in terms of ms defined by the TIMEOUT variable,
 * otherwise panic() is called.
 * Author: Alessio
 */
void check_alive_connection() {
	int32_t current_ms, diff;

	if(X32_ms_last_packet == -1) return; //does not perform the check untill a new message arrives

	if(X32_ms_clock - X32_ms_last_packet > TIMEOUT && mode >= MANUAL)
		set_mode(PANIC);
}

void check_msg_q(Fifo *q, void (*callback)(char, PacketData), void (*error)()){
	while(fifo_size(q) >= 4) { // Check if there are one or more packets in the queue
		char control;
		PacketData data;
		char checksum;

		fifo_peek_at(q, &control, 0);
		fifo_peek_at(q, &data.as_bytes[0], 1);
		fifo_peek_at(q, &data.as_bytes[1], 2);
		fifo_peek_at(q, &checksum, 3);

		if(!check_packet(control,data,checksum)) {
			// If the checksum is not correct, pop the first message off the queue and repeat the loop
			char c;
			fifo_pop(q, &c);

			error();
		} else {
			// If the checksum is correct, pop the packet off the queue and notify a callback
			char c;
			fifo_pop(q, &c);
			fifo_pop(q, &c);
			fifo_pop(q, &c);
			fifo_pop(q, &c);

			callback(control, data);
		}
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
		check_msg_q(&pc_msg_q, &packet_received, &lost_packet);
		ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Re-enable messages from the PC after processing them
	}

	quit();

	return 0;
}
