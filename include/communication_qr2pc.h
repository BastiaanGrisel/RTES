#ifndef COMMUNICATION_QR2PC_H
#define COMMUNICATION_QR2PC_H

#include "communication.h"
#include "types.h"

void rs232_putchar(char c) {
	putchar(c);
}

/*Error function that send all the error messages defined in types.h
Author: Alessio */
void send_error(Error err)
{
	PacketData p;
	p.as_int16_t = err;
	send_packet(rs232_putchar, ERROR_MSG, p); //Sending error code
}

/* Send something to the terminal
 * Author: Henko
 */
void send_term_message(char message[]) {
	send_control(rs232_putchar, TERMINAL_MSG_START); // begin terminal message
	send_string(rs232_putchar, TERMINAL_MSG_PART, message);
	send_control(rs232_putchar, TERMINAL_MSG_FINISH); // end terminal message
}

#endif
