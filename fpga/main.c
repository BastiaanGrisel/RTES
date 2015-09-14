/*------------------------------------------------------------------
 *  main.c -- derived from demo-projects/leds.c copy buttons to leds
 *            (no isr)
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include "x32.h"

/* define some peripheral short hands
 */
#define X32_leds		peripherals[PERIPHERAL_LEDS]
#define X32_buttons		peripherals[PERIPHERAL_BUTTONS]
#define X32_clock		peripherals[PERIPHERAL_MS_CLOCK]

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
typedef enum {false,true} bool;
enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } mode = SAFE;

int		isr_qr_time = 0, isr_qr_counter =0;
char	control; //is the first byte of every packet, it defines the type of the packet.
int		ae[4];
int 	offset[4];
int 	R=0, P=0, Y=0, T=0;
int		s0, s1, s2, s3, s4, s5;
bool	expect_value = false, new_user_input= false, sensor_active = false;

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
}

/*
 * Changes the mode to either: SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL or FULL_CONTROL.
 * Motor RPM needs to be zero to change mode except when changing to SAFE and PANIC
 * Returns a boolean indicating whether the mode change was succesful or not.
 * Author: Alessio
 */
bool set_mode(char new_mode)
{
  new_mode -= '0'; /*Turn the char into an int. It is guaranteed by the standard to work.
	 								  Maybe this isn't necessary if we find where - during the communication - we read chars instead of ints.*/

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
			add_motor_offset(
				+OFFSET_STEP,
				+OFFSET_STEP,
				+OFFSET_STEP,
				+OFFSET_STEP
				);
			break;
		case 'z': // throttle down
			add_motor_offset(
				-OFFSET_STEP,
				-OFFSET_STEP,
				-OFFSET_STEP,
				-OFFSET_STEP
				);
			break;
		case LEFT_CHAR: // roll left
			add_motor_offset(
				0,
				+OFFSET_STEP,
				0,
				-OFFSET_STEP
				);
			break;
		case RIGHT_CHAR: // roll right
			add_motor_offset(
				0,
				-OFFSET_STEP,
				0,
				+OFFSET_STEP
				);
			break;
		case UP_CHAR: // pitch up
			add_motor_offset(
				-OFFSET_STEP,
				0,
				+OFFSET_STEP,
				0
				);
			break;
		case DOWN_CHAR: // pitch down
			add_motor_offset(
				+OFFSET_STEP,
				0,
				-OFFSET_STEP,
				0
				);
			break;
		case 'q': // yaw left
			add_motor_offset(
				-OFFSET_STEP,
				+OFFSET_STEP,
				-OFFSET_STEP,
				+OFFSET_STEP
				);
			break;
		case 'w': // yaw right
			add_motor_offset(
				+OFFSET_STEP,
				-OFFSET_STEP,
				+OFFSET_STEP,
				-OFFSET_STEP
				);
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
			printf("#\xFE%c%c%c%c\n",offset[0]/10,offset[1]/10,offset[2]/10,offset[3]/10);
			printf("\xFF%c%c%c%c\n",ae[0]/10,ae[1]/10,ae[2]/10,ae[3]/10);
			break;
	}
}

/* set the value to the right place
 * Author: Henko
 */
void set_value(char c){
	switch(control){
		case 'M':
			if(set_mode(c))
			  puts("Mode succesfully changed.");
			else
			   puts("Invalid or not permitted mode!");

		  printf("#Control: >%c<, Current Mode: >%i<\n",control,mode);
			break;
		case 'R':
			R = c;
			break;
		case 'P':
			P = c;
			break;
		case 'Y':
			Y = c;
			break;
		case 'T':
			T = c;
			break;
		case 'A':
			trim(c);
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
	//printf("Intrpt:");

	// Read the data of serial comm
	c = X32_rs232_data;

	if(!expect_value){
		control = c;
		expect_value = true;
	}
	else // expect a value
	{
		set_value(c);
		new_user_input= true;
		expect_value = false; // reset expect_value
		//printf("#Control: >%c<, Mode: >%i<\n",control,mode);
		//X32_leds = 1<<mode;
	}

	if(new_user_input & !sensor_active){
		new_user_input = false;
		ae[0] = offset[0]+T  +P+Y;
		ae[1] = offset[1]+T-R  -Y;
		ae[2] = offset[2]+T  -P+Y;
		ae[3] = offset[3]+T+R  -Y;
	}
}



/*------------------------------------------------------------------
 * isr_qr_link -- QR link rx interrupt handler
 *------------------------------------------------------------------
 */
void isr_qr_link(void)
{
	int	ae_index;
	/* record time
	 */
	/*isr_qr_time = X32_us_clock;
        inst = X32_instruction_counter;*/
	/* get sensor and timestamp values
	 */
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2;
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;
	//timestamp = X32_QR_timestamp;

	/* monitor presence of interrupts
	 */
	isr_qr_counter++;
	if (isr_qr_counter % 500 == 0) {
		toggle_led(2);
		sensor_active = true;
	}


	/*
	 * calculate engine values
	 */
	 if(new_user_input){
		new_user_input = false;
		ae[0] = offset[0]+T  +P+Y;
		ae[1] = offset[1]+T-R  -Y;
		ae[2] = offset[2]+T  -P+Y;
		ae[3] = offset[3]+T+R  -Y;
	}


	/* Clip engine values to be positive and 10 bits.
	 */
	for (ae_index = 0; ae_index < 4; ae_index++)
	{
	/*	if (ae[ae_index] < 0)
			ae[ae_index] = 0;*/
		 ae[ae_index] = (ae[ae_index] < 0) ? 0 : ae[ae_index];

		ae[ae_index] &= 0x3ff;
	}

	/* Send actuator values
	 * (Need to supply a continous stream, otherwise
	 * QR will go to safe mode, so just send every ms)
	 */
	X32_QR_a0 = ae[0];
	X32_QR_a1 = ae[1];
	X32_QR_a2 = ae[2];
	X32_QR_a3 = ae[3];

	/* record isr execution time (ignore overflow)
	 */
       /* inst = X32_instruction_counter - inst;
	isr_qr_time = X32_us_clock - isr_qr_time;*/
}

int main()
{
	int c;
	printf("Program started in mode: %d \r\n", mode);

	/* Initialize Variables
	 */

	isr_qr_counter = isr_qr_time = 0;
	ae[0] = ae[1] = ae[2] = ae[3] = 0;
	offset[0] = offset[1] = offset[2] = offset[3] =0;

	/* Prepare Interrupts
	 */

	// FPGA -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_XUFO, &isr_qr_link);
    	SET_INTERRUPT_PRIORITY(INTERRUPT_XUFO, 21);

	// PC -> X32
	SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_RX, &isr_rs232_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_RX, 20);
	while (X32_rs232_char) c = X32_rs232_data; // empty the buffer

	/* Enable Interrupts
	 */

    	ENABLE_INTERRUPT(INTERRUPT_XUFO);
        ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX);

	/* Enable all interupts after init code
	 */
	ENABLE_INTERRUPT(INTERRUPT_GLOBAL);

	// Main loop
	while (1) {
		// Turn on the LED corresponding to the mode
		X32_leds = 1 << mode;
	}

	/* Exit routine
	*/
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);
	return 0;
}
