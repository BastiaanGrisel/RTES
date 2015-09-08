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

char c;

/*
 * Process interupts from serial connection
 * Author: Bastiaan Grisel
 */
void isr_rs232_rx(void) 
{
	//printf("Interrupt");

	char d;
	char m;

	if((d = X32_rs232_data) == 'M')
		if(m = X32_rs232_data)
			printf("Ctrl: %c, Mode: %c\n",c,m);
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
