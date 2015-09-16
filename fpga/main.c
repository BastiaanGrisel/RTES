/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include "x32.h"

#include "types.h"
#include "queue.h"
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
#define X32_QR_timestamp 	peripherals[PERIPHERAL_XUFO_TIMESTAMP]

#define X32_rs232_data		peripherals[PERIPHERAL_PRIMARY_DATA]
#define X32_rs232_stat		peripherals[PERIPHERAL_PRIMARY_STATUS]
#define X32_rs232_char		(X32_rs232_stat & 0x02)

/* User defines
 */
#define LEFT_CHAR 'f'
#define RIGHT_CHAR 'h'
#define UP_CHAR 't'
#define DOWN_CHAR 'g'

#define OFFSET_STEP 10

/* Define global variables
 */
char malloc_memory[1024];
int malloc_memory_size = 1024;

int	isr_qr_time = 0, isr_qr_counter =0;
int	ae[4];
int 	offset[4];
int 	R=0, P=0, Y=0, T=0;
int	s0, s1, s2, s3, s4, s5;
Queue	pc_msg_q;

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

/* Reset the offsets.
   Author: Alessio
*/
void reset_motors()
{
	offset[0] = offset[1] = offset[2] = offset[3] = 0;
	R= P= Y= T=0;
}

/*
 * Changes the mode to either: SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL or FULL_CONTROL.
 * Motor RPM needs to be zero to change mode except when changing to SAFE and PANIC
 * Returns a boolean indicating whether the mode change was succesful or not.
 * Author: Alessio
 */
bool set_mode(int new_mode)
{
	if(new_mode < SAFE || new_mode > FULL_CONTROL) return false;

	if(new_mode >= MANUAL) {
		// If at least one of the motor's RPM is not zero, return false
		int i;
		for(i = 0; i < 4; i++)
			if(ae[i] > 0) return false;
	}

	mode = new_mode;
	return true;
}

void toggle_led(int i)
{
	X32_leds = (X32_leds ^ (1 << i));
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
		case 'j':
		case 'i':
		case 'k':
		case 'o':
		case 'l':
		default:
			printf("offset = [%c%c%c%c]\n",offset[0]/10,offset[1]/10,offset[2]/10,offset[3]/10);
			printf("ae = [%c%c%c%c]\n#",ae[0]/10,ae[1]/10,ae[2]/10,ae[3]/10);
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

	// Read the data of serial comm
	c = X32_rs232_data;

	// Add the message to the message queue
	pc_msg_q.push(&pc_msg_q, c);

	isr_qr_time = X32_us_clock - isr_qr_time; //data to be logged
}

/* ISR when new sensor readings are read from the QR
 */
void isr_qr_link(void)
{
	int ae_index;

	isr_qr_time = X32_us_clock;

	/* TODO: do filtering here?
	 * get sensor and timestamp values */
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;

	/* Where do we need this for?
	isr_qr_counter++;
	if (isr_qr_counter % 500 == 0) {
		toggle_led(2);
		sensor_active = true;
	}*/

	isr_qr_time = X32_us_clock - isr_qr_time; // why does this happen her and also at the end of the other ISR?
}

void set_motor_rpm(int motor0, int motor1, int motor2, int motor3) {
	/* TODO: Does this code belong here?
	 * Clip engine values to be positive and 10 bits.
	 */
	int ae_index;
	for (ae_index = 0; ae_index < 4; ae_index++)
	{
		if (ae[ae_index] < 0)
			ae[ae_index] = 0;
//		ae[ae_index] = (ae[ae_index] < 0) ? 0 : ae[ae_index];

		ae[ae_index] &= 0x3ff;
	}

	/* Send actuator values
	 * (Need to supply a continous stream, otherwise
	 * QR will go to safe mode, so just send every ms)
	 */
	X32_QR_a0 = motor0;
	X32_QR_a1 = motor1;
	X32_QR_a2 = motor2;
	X32_QR_a3 = motor3;
}

/* The startup routine.
 * Originally created by: Bastiaan
 */
void setup()
{
	int c;

	/* Initialize Variables */

	isr_qr_counter = isr_qr_time = 0;
	ae[0] = ae[1] = ae[2] = ae[3] = 0;
	offset[0] = offset[1] = offset[2] = offset[3] =0;
	pc_msg_q = createQueue();

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

void packet_received(char control, char value) {
	printf("Message Received: %d %d\n", control, value);

	/*if(new_user_input & !sensor_active){
		new_user_input = false;
		ae[0] = offset[0]+T  +P+Y;
		ae[1] = offset[1]+T-R  -Y;
		ae[2] = offset[2]+T  -P+Y;
		ae[3] = offset[3]+T+R  -Y;
	}*/

	switch(control){
		case 'M':
	//	control = control - '0'; //leave this here just for trying with myterm.c when kj.o is not working @Alessio
		if(set_mode(value))
			printf("Mode succesfully changed.\n");
		else
			printf("Invalid or not permitted mode!\n");

		printf("Control: >%c<, Current Mode: >%i<\n#",control,mode);
		break;
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
			T = value;
			break;
		case 'A':
			trim(value);
			break;
		default:
			break;
	}
}

int main()
{
	printf("Program started in mode: %d \r\n#", mode);

	setup();

	// Main loop
	while (1) {
		// Turn on the LED corresponding to the mode
		X32_leds = 1 << mode;

		// Process messages
        	DISABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Disable incoming messages while working with the message queue

		while(pc_msg_q.size >= 3) { // Check if there are one or more packets in the queue
			char control  = pc_msg_q.peek(&pc_msg_q, 0);
			char value    = pc_msg_q.peek(&pc_msg_q, 1);
			char checksum = pc_msg_q.peek(&pc_msg_q, 2);

			if(!check_packet(control,value,checksum)) {
				// If the checksum is not correct, pop the first message off the queue and repeat the loop
				pc_msg_q.pop(&pc_msg_q);
			} else {
				// If the checksum is correct, pop the packet off the queue and notify a callback
				pc_msg_q.pop(&pc_msg_q);
				pc_msg_q.pop(&pc_msg_q);
				pc_msg_q.pop(&pc_msg_q);

				packet_received(control,value);
			}
		}

		ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX); // Re-enable messages from the PC after processing them

		// Calculate motor RPM
	}

	quit();

	return 0;
}
