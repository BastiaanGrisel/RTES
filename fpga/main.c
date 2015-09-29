/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>

#include "x32.h"
#include "checksum.h"
#include "logging.h"

/* define some peripheral short hands
 */
#define X32_leds		peripherals[PERIPHERAL_LEDS]
#define X32_buttons		peripherals[PERIPHERAL_BUTTONS]
#define X32_ms_clock		peripherals[PERIPHERAL_MS_CLOCK]
#define X32_us_clock 	(X32_ms_clock *1000)

#define X32_QR_a0 		peripherals[PERIPHERAL_XUFO_A0]
#define X32_QR_a1 		peripherals[PERIPHERAL_XUFO_A1]
#define X32_QR_a2 		peripherals[PERIPHERAL_XUFO_A2]
#define X32_QR_a3 		peripherals[PERIPHERAL_XUFO_A3]
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

/* Define global variables
 */

int32_t  X32_ms_last_packet = -1; //ms of the last received packet. Set to -1 to avoid going panic before receiving the first msg
int32_t  time_at_last_led_switch = 0;
int32_t  packet_counter = 0, packet_lost_counter = 0;
int32_t	 isr_qr_time = 0, isr_qr_counter = 0;
int32_t  offset[4];
int32_t  R=0, P=0, Y=0, T=0, js_T=0;

/* filter parameters*/
int32_t  Ybias = 0;
int32_t  filtered_dY = 0; //
int32_t  Y_BIAS_UPDATE = 14; // update bias each sample with a fraction of 1/2^13
int32_t  Y_FILTER = 4; // simple filter that updates 1/2^Y_filter
int32_t  P_yaw=4; // P = 2^4     Y_TO_ENGINE_SCALE
int32_t  P_roll=4;
int32_t  P_pitch=4;
int32_t  Y_stabilize;

/* sensor values */
int32_t	s0, s1, s2, s3, s4, s5;
int32_t sensor_log_counter = 0;
uint32_t sensor_log[LOG_SIZE][7];

Fifo	pc_msg_q; // message que received from pc
char message[100]; // message to send to pc-terminal

Mode	mode = SAFE;
Loglevel log_level = SENSORS;

/* function declarations TODO put this in header */
void panic(void);


/* Add offset to the four motors
 * No need to check for negative numbers since offset can be negative
 * Author: Bastiaan
 */
void add_motor_offset(int32_t motor0, int32_t motor1, int32_t motor2, int32_t motor3)
{
	offset[0] += motor0;
	offset[1] += motor1;
	offset[2] += motor2;
	offset[3] += motor3;
}

void update_nexys_display(){
	nexys_display = packet_counter << 8 + packet_lost_counter;
	///nexys_display = [debugValue];
}

void lost_packet()
{
	packet_lost_counter++;
	update_nexys_display();
}

void set_motor_rpm(int32_t motor0, int32_t motor1, int32_t motor2, int32_t motor3) {
	/* TODO: Arguments should be floats if we have them
	 * Clip engine values to be positive and 10 bits.
	 */
	motor0 = (motor0 < 0 ? 0 : motor0) & 0x3ff;
	motor1 = (motor1 < 0 ? 0 : motor1) & 0x3ff;
	motor2 = (motor2 < 0 ? 0 : motor2) & 0x3ff;
	motor3 = (motor3 < 0 ? 0 : motor3) & 0x3ff;

	/* Send actuator values
	 * (Need to supply a continous stream, otherwise
	 * QR will go to safe mode, so just send every ms)
	 */
	X32_QR_a0 = motor0;
	X32_QR_a1 = motor1;
	X32_QR_a2 = motor2;
	X32_QR_a3 = motor3;
}

/* Reset the offsets.
   Author: Alessio
*/
void reset_motors()
{
	offset[0] = offset[1] = offset[2] = offset[3] = 0;
	R = P = Y = T = 0;
	set_motor_rpm(0,0,0,0);
}


/*
 * Changes the mode to either: SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL or FULL_CONTROL.
 * Motor RPM needs to be zero to change mode except when changing to SAFE and PANIC
 * Returns a boolean indicating whether the mode change was succesful or not.
 * Author: Alessio
 */
bool set_mode(int32_t new_mode)
{
	/* Make sure that the mode is in bounds */
	if(new_mode < SAFE || new_mode > FULL_CONTROL) {
		send_err_message(MODE_ILLIGAL);
		return false;
	}

	/* Make sure that a change to an operational mode can only be done from SAFE */
	if(mode >= MANUAL && new_mode >= MANUAL){
		send_err_message(MODE_CHANGE_ONLY_VIA_SAFE);
		return false;
	}

	if(mode == new_mode) {
		send_err_message(MODE_ALREADY_SET);
		return false;
	}

	if(new_mode >= MANUAL) {
		// If at least one of the motor's RPM is not zero, return false
		int32_t i;
		for(i = 0; i < 4; i++)
			if(get_motor_rpm(i) > 0) {
				send_err_message(MODE_CHANGE_ONLY_IF_ZERO_RPM);
				//sprintf(message, "\n RPM= [%i%i%i%i] ",get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
				return false;
			}

		// Also make sure that the throttle is in the zero position
		if(js_T != 0) {
			sprintf(message, "js_T: %i \n", js_T);
			send_err_message(MODE_CHANGE_ONLY_IF_ZERO_RPM);
			return false;
		}
	}

	/* If everything is OK, change the mode */
	mode = new_mode;
	sprintf(message, "Succesfully changed to mode: >%i< ", new_mode);

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
			panic();
			break;
		case ASK_MOTOR_RPM:
			sprintf(message, "offset = [%i%i%i%i]\nmotor RPM= [%i%i%i%i]\n",offset[0],offset[1],offset[2],offset[3],get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
			send_term_message(message);
			break;
		case ASK_FILTER_PARAM:
			sprintf(message, "Y_stabilize = %i,  Ybias = %i, filtered_dY = %i\n#",Y_stabilize,Ybias, filtered_dY);
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
	int32_t dY;
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
	}*/

	/*YAW_CALCULATIONS*/
	dY 		= (s5 << Y_BIAS_UPDATE) - Ybias; 		// dY is now scaled up with Y_BIAS_UPDATE
	Ybias   	+= -1 * (Ybias >> Y_BIAS_UPDATE) + s5; 		// update Ybias with 1/2^Y_BIAS_UPDATE each sample
	filtered_dY 	+= -1 * (filtered_dY >> Y_FILTER) + dY; 	// filter dY
	//Y +=filtered_dY;						// int32_tegrate dY to get yaw (but if I remem correct then we need the rate not the yaw)
	Y_stabilize 	= (0 - filtered_dY) >> (Y_BIAS_UPDATE - P_yaw); // calculate error of yaw rate
	if(mode == YAW_CONTROL) {

	}

	if(mode >=MANUAL){
		// Calculate motor RPM
		set_motor_rpm(
			offset[0] + T  +P+Y,
			offset[1] + T-R  -Y,
			offset[2] + T  -P+Y,
			offset[3] + T+R  -Y);
	}

	isr_qr_time = X32_us_clock - isr_qr_time; // why does this happen here and also at the end of the other ISR?
}


/* Gets the RPM of a certain motor.
 * Author: Bastiaan
 */
int32_t get_motor_rpm(int32_t i) {
	switch(i) {
		case 0: return X32_QR_a0;
		case 1: return X32_QR_a1;
		case 2: return X32_QR_a2;
		case 3: return X32_QR_a3;
		default: return 0;
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
	//sprintf(message, "Packet Received: %c %c\n#", control, data.as_char);
	//send_term_message(message);

	/* Make sure that the throttle is zero before changing the mode */
	if(control == JS_LIFT)
		js_T = data.as_int8_t;

	if(mode < MANUAL && !(control == MODE_CHANGE || control == ADJUST || control == SPECIAL_REQUEST)){
		sprintf(message, "[%c %i] Change mode to operate the QR!\n", control, data.as_char);
		send_term_message(message);
		return;
	}

	switch(control){
		case MODE_CHANGE:
			set_mode(data.as_char);
			send_term_message(message);
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
				T = data.as_int8_t;
			else
				T = 256 + data.as_int8_t;

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
	offset[0] = offset[1] = offset[2] = offset[3] =0;
	fifo_init(&pc_msg_q);

  init_array(sensor_log);

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

/* Set panic mode and provide a soft landing.
* Then resets the motors and quits.
 * Author: Alessio
 */
void panic() {

	X32_ms_last_packet = -1;
	set_mode(PANIC);
	set_motor_rpm(100,100,100,100);
	nexys_display = 0x0000;
	X32_sleep(500);
	nexys_display = 0xC000;
	X32_sleep(1000);
  	nexys_display = 0xC100;
	X32_sleep(1000);
	reset_motors();
	nexys_display = 0xc1a0;
	X32_sleep(1000);
}

/* checks if the QR is receiving packet in terms of ms defined by the TIMEOUT variable,
 * otherwise panic() is called.
 * Author: Alessio
 */
void check_alive_connection() {
	int32_t current_ms, diff;

	if(X32_ms_last_packet == -1) return; //does not perform the check untill a new message arrives

	current_ms = X32_ms_clock;

	if(current_ms - X32_ms_last_packet > TIMEOUT)
	{
		panic();
	}

	return;
}



int32_t main(void)
{
	setup();

	// Main loop
	while (1) {
		// Ping the PC
	   	check_alive_connection();

		// Turn on the LED corresponding to the mode and don't change led 6 and 7
		X32_leds = ((flicker_slow()?1:0) << mode) | (X32_leds & 0xC0);

		// Process messages
        	DISABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Disable incoming messages while working with the message queue

		while(fifo_size(&pc_msg_q) >= 4) { // Check if there are one or more packets in the queue
			char control;
			PacketData data;
			char checksum;

			fifo_peek_at(&pc_msg_q, &control, 0);
			fifo_peek_at(&pc_msg_q, &data.as_bytes[0], 1);
			fifo_peek_at(&pc_msg_q, &data.as_bytes[1], 2);
			fifo_peek_at(&pc_msg_q, &checksum, 3);

			if(!check_packet(control,data,checksum)) {
				// If the checksum is not correct, pop the first message off the queue and repeat the loop
				char c;
				fifo_pop(&pc_msg_q, &c);

				lost_packet();
			} else {
				// If the checksum is correct, pop the packet off the queue and notify a callback
				char c;
				fifo_pop(&pc_msg_q, &c);
				fifo_pop(&pc_msg_q, &c);
				fifo_pop(&pc_msg_q, &c);
				fifo_pop(&pc_msg_q, &c);

				if(control != 0){
					packet_received(control, data);
				}
			}
		}

		ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Re-enable messages from the PC after processing them

		// give a signal when sensors are ready
		if(sensor_log_counter >= 10000) {
			X32_leds = X32_leds | 0x90; // 1000000 = enable led 7
		}
	}

	quit();

	return 0;
}
