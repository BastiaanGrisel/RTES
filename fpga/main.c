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

#define X32_rs232_data		peripherals[PERIPHERAL_PRIMARY_DATA]
#define X32_rs232_stat		peripherals[PERIPHERAL_PRIMARY_STATUS]
#define X32_rs232_char		(X32_rs232_stat & 0x02)

enum { SAFE, PANIC, MANUAL, CALIBRATE, YAW_CONTROL, FULL_CONTROL } mode = SAFE;
typedef enum {false,true} bool;

char c;
char control;
char value;
bool expect_value = false;

void toggle_led(int i) 
{
	X32_leds = (X32_leds ^ (1 << i));
}

/*
 * Process interrupts from serial connection
 * Author: Bastiaan Grisel
 */
void isr_rs232_rx(void) 
{
	printf("Interrupt");

	// Read the data of serial comm
	c = X32_rs232_data;

	if(c == 'M' && !expect_value)
	{
		control = c;
		expect_value = true;
	}
	else if(expect_value)
	{
		value = c;
		expect_value = false;
		printf("Ctrl: >%c<, Mode: >%i<\n",control,value);
		mode = value - '0';
	}
}

int main() 
{
	printf("Program started in mode: %d \r\n", mode);

	// prepare rs232 rx interrupt and getchar handler
        SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_RX, &isr_rs232_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_RX, 20);
	while (X32_rs232_char) c = X32_rs232_data; // empty the buffer
        ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX);

	// Enable all interupts after init code
	ENABLE_INTERRUPT(INTERRUPT_GLOBAL); 

	while (1) {
		// Turn on the LED corresponding to the mode
		X32_leds = 1 << mode;

		if (X32_buttons == 0x09)
			break;
	}
	
	// Disable interrupts before exiting
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL); 
	return 0;
}
