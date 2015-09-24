/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include "x32.h"

#include "types.h"
#include "fifo.h"
#include "checksum.h"

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
#define LOG_SIZE 10000

/* Define global variables
 */
char malloc_memory[1024];
int malloc_memory_size = 1024;

int 	time_at_last_led_switch = 0;
int 	X32_ms_last_packet= -1; //ms of the last received packet. 1s for booting up and starting sending values
int 	packet_counter = 0, packet_lost_counter = 0;
int	isr_qr_time = 0, isr_qr_counter = 0;
int 	offset[4];
int 	R=0, P=0, Y=0, T=0;

/* filter parameters*/
int	Ybias = 0;
int 	filtered_dY = 0; //
int 	Y_BIAS_UPDATE = 14; // update bias each sample with a fraction of 1/2^13
int 	Y_FILTER = 4; // simple filter that updates 1/2^Y_filter
int 	P_yaw=4; // P = 2^4     Y_TO_ENGINE_SCALE
int 	Y_stabilize;

int	s0, s1, s2, s3, s4, s5;
Fifo	pc_msg_q;
Mode	mode = SAFE;
Loglevel log_level = SENSORS;

int sensor_log[LOG_SIZE][7];
int sensor_log_counter = 0;
char message[100];

/* Add offset to the four motors
 * No need to check for negative numbers since offset can be negative
 * Author: Bastiaan
 */
void add_motor_offset(int motor0, int motor1, int motor2, int motor3)
{
	offset[0] += motor0;
	offset[1] += motor1;
	offset[2] += motor2;
	offset[3] += motor3;
}

void update_nexys_display(){
	nexys_display = packet_counter<<8 + packet_lost_counter;
	///nexys_display = [debugValue];
}

void lost_packet()
{
	packet_lost_counter++;
	update_nexys_display();
}

void set_motor_rpm(int motor0, int motor1, int motor2, int motor3) {
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

void init_array()
{
	int i;
	for(i=0; i < LOG_SIZE; i++)
  {
 		sensor_log[i][0] = 20000;//X32_QR_timestamp/50;
 		sensor_log[i][1] = 5;//s0;
 		sensor_log[i][2] = 5;//s1;
 		sensor_log[i][3] = 5;//s2;
 		sensor_log[i][4] = 5;//s3;
 		sensor_log[i][5] = 5;//s4;
 		sensor_log[i][6] = 5;//s5;
 }
}

/* send a complete message
 * Author: Henko Aantjes
 */
void send_message(char control, char value){
	putchar(control);
	putchar(value);
	putchar(checksum(control,value));
}

/* send a sequence of messages, for example: write log or to terminal
 *
 */
void send_long_message(char control, char message[]){
	int i;
	for (i = 0; message[i] != 0; i++){
		send_message(control, message[i]);
	}
}

/* send something to the terminal
 *
 */
void send_term_message(char message[]){
	send_message('B', 'T'); // begin terminal message
	send_long_message('T', message);
	send_message('F', 'T'); // end terminal message
}

/*
 * Changes the mode to either: SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL or FULL_CONTROL.
 * Motor RPM needs to be zero to change mode except when changing to SAFE and PANIC
 * Returns a boolean indicating whether the mode change was succesful or not.
 * Author: Alessio
 */
bool set_mode(int new_mode)
{
	if(new_mode < SAFE || new_mode > FULL_CONTROL) {
		sprintf(message, "[M %i]= invalid mode, current mode =>%i< ", new_mode,mode);
		return false;
	}

	if(new_mode >= MANUAL) {
		// If at least one of the motor's RPM is not zero, return false
		int i;
		for(i = 0; i < 4; i++)

			if(get_motor_rpm(i) > 0) {
				sprintf(message, "[M %i]=not allowed! Motors are turning! motor RPM= [%i%i%i%i] ", new_mode,get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
				return false;
			}

		reset_motors();
	}

	if(mode >=MANUAL && new_mode>= MANUAL){
		sprintf(message, "[M %i]= invalid mode change (first go to SAFE mode), current mode =>%i< ", new_mode,mode);
		return false;
	}
	mode = new_mode;
	sprintf(message, "[M %i] Succesfully changed to mode:>%i< ", new_mode,mode);

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
		case LEFT_CHAR: // roll left
			add_motor_offset(0, +OFFSET_STEP, 0, -OFFSET_STEP);
			break;
		case RIGHT_CHAR: // roll right
			add_motor_offset(0, -OFFSET_STEP, 0, +OFFSET_STEP);
			break;
		case UP_CHAR: // pitch up
			add_motor_offset(-OFFSET_STEP, 0, +OFFSET_STEP, 0);
			break;
		case DOWN_CHAR: // pitch down
			add_motor_offset(+OFFSET_STEP, 0, -OFFSET_STEP, 0);
			break;
		case 'q': // yaw left
			add_motor_offset(-OFFSET_STEP, +OFFSET_STEP, -OFFSET_STEP, +OFFSET_STEP);
			break;
		case 'w': // yaw right
			add_motor_offset(+OFFSET_STEP, -OFFSET_STEP, +OFFSET_STEP, -OFFSET_STEP);
			break;
		case 'r': //reset
			reset_motors();
			break;
		case 'u':
			P_yaw++;
			break;
		case 'j':
			P_yaw--;
			break;
		case 'i':
			break;
		case 'k':
			break;
		case 'o':
			break;
		case 'l':
			break;
		case 'm':
			sprintf(message, "offset = [%i%i%i%i]\nmotor RPM= [%i%i%i%i]\n",offset[0]/10,offset[1]/10,offset[2]/10,offset[3]/10,get_motor_rpm(0)/10,get_motor_rpm(1)/10,get_motor_rpm(2)/10,get_motor_rpm(3)/10);
			send_term_message(message);
			break;
		case 'f':
			sprintf(message, "fix = %i,  Ybias = %i, filtered_dY = %i\n#",Y_stabilize,Ybias, filtered_dY);
			send_term_message(message);
			break;
		case 's':
			sensor_log_counter = 0;
			X32_leds = X32_leds & 0x7F; // 01111111 = disable led 7
			break;
		default:
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
	int dY;
	isr_qr_time = X32_us_clock;

	/* get sensor and timestamp values */
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;

	if(sensor_log_counter < LOG_SIZE) {
		sensor_log[sensor_log_counter][0] = X32_QR_timestamp/50;
		sensor_log[sensor_log_counter][1] = s0;
		sensor_log[sensor_log_counter][2] = s1;
		sensor_log[sensor_log_counter][3] = s2;
		sensor_log[sensor_log_counter][4] = s3;
		sensor_log[sensor_log_counter][5] = s4;
		sensor_log[sensor_log_counter][6] = s5;
		sensor_log_counter++;
	}

	/*YAW_CALCULATIONS*/
	dY 		= (s5 << Y_BIAS_UPDATE) - Ybias; 		// dY is now scaled up with Y_BIAS_UPDATE
	Ybias   	+= -1 * (Ybias >> Y_BIAS_UPDATE) + s5; 		// update Ybias with 1/2^Y_BIAS_UPDATE each sample
	filtered_dY 	+= -1 * (filtered_dY << Y_FILTER) + dY; 	// filter dY
	//Y +=filtered_dY;						// integrate dY to get yaw (but if I remem correct then we need the rate not the yaw)
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
int get_motor_rpm(int i) {
	switch(i) {
		case 0: return X32_QR_a0;	
		case 1: return X32_QR_a1;
		case 2: return X32_QR_a2;
		case 3: return X32_QR_a3;
		default: return 0;
	}
}

/* Send over the logs that are stored in 'sensor_log'
 */
void send_logs() {
	int i;
	int j;

	for(i = 0; i < 10000; i++) {
		for(j = 0; j < 7; j++) {
			char low  =  sensor_log[i][j]       & 0xff;
			char high = (sensor_log[i][j] >> 8) & 0xff;

			send_message('L',high);
			send_message('L',low);
		}

		send_message('L','~');

		if(i%100==99){
			sprintf(message, "%i",i/100+1);
			send_term_message(message);
		}
	}
	send_term_message("LOGGING COMPLETED");
}

/* Callback that gets executed when a packet has arrived
 * Author: Bastiaan
 */
void packet_received(char control, char value) {
	//sprintf(message, "Packet Received: %c %i\n#", control, value);
	//send_term_message(message);
	if(mode<MANUAL && (control!='M' && control!= 'A' && control!= 'L')){
		sprintf(message, "[%c %i] Change mode to operate the QR!\n",control, value);
		send_term_message(message);
		return;
	}

	switch(control){
		case 'M':
			set_mode(value);
			send_term_message(message);
			break;
		case 'R':
			R = value;
			break;
		case 'P':
			P = value;
			break;
		case 'Y':
			Y = value;
			break;
		case 'T':
			if(value >= 0)
				T = value;
			else
				T = 256 + value;
			break;
		case 'A':
			trim(value);
			break;
		case 'L':
			if(mode == SAFE)
				send_logs();
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
	int c;

	/* Initialize Variables */

	isr_qr_counter = isr_qr_time = 0;
	offset[0] = offset[1] = offset[2] = offset[3] =0;
	fifo_init(&pc_msg_q);

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
void X32_sleep(int millisec) {
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
	int current_ms, diff;

	if(X32_ms_last_packet == -1) return; //does not perform the check untill a new message arrives

	current_ms = X32_ms_clock;

	if(current_ms - X32_ms_last_packet > TIMEOUT)
	{
		panic();
	}

	return;
}



int main(void)
{
	setup();
	//init_array(); //for testing the logging output
  	nexys_display = 0x00;

	// Main loop
	while (1) {
		// Ping the PC
	   	//check_alive_connection();

		// Turn on the LED corresponding to the mode and don't change led 6 and 7
		X32_leds = ((flicker_slow()?1:0) << mode) | (X32_leds & 0xC0);

		// Process messages
        	DISABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Disable incoming messages while working with the message queue

		while(fifo_size(&pc_msg_q) >= 3) { // Check if there are one or more packets in the queue
			char control;
			char value;
			char checksum;

			fifo_peek_at(&pc_msg_q, &control, 0);
			fifo_peek_at(&pc_msg_q, &value, 1);
			fifo_peek_at(&pc_msg_q, &checksum, 2);

			if(!check_packet(control,value,checksum)) {
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
				if(control != 0){
					packet_received(control,value);
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
