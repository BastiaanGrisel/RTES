#ifndef QRMESSAGE_H
#define QRMESSAGE_H

#include "communication.h"
#include "types.h"
#include "fifo.h"

/************** QR --> PC ************/

/* send a complete message
 * Author: Henko Aantjes
 */
void send_message(char control, PacketData data){
	putchar(control);
	putchar(data.as_bytes[0]);
	putchar(data.as_bytes[1]);
	putchar(checksum(control, data));
}

/* send a message that doesn't need a value
 * Author: Henko Aantjes
 */
void send_control_message(char control){
	send_message(control, ch2pd(NOT_IMPORTANT));
}

/* send a sequence of messages, for example: write log or to terminal
 * Author: Henko
 */
void send_long_message(char control, char message[]){
	int i;
	for (i = 0; message[i] != 0; i++){
		send_message(control, ch2pd(message[i]));
	}
}

/* send something to the terminal
 * Author: Henko
 */
void send_term_message(char message[]){
	send_control_message(TERMINAL_MSG_START); // begin terminal message
	send_long_message(TERMINAL_MSG_PART, message);
	send_control_message(TERMINAL_MSG_FINISH); // end terminal message
}

/*Error function that send all the error messages defined in types.h
Author: Alessio */
void send_err_message(Error err)
{
	PacketData p;
	p.as_uint16_t = err;
	send_message(ERROR_MSG,p); //Sending error code
}

/*Send the feedback message from the QR, with all the data required.
Author: Alessio*/
void send_feedback_message(char message[])
{
	send_long_message(FB_MSG,message);
	send_control_message(FB_MSG_END);
}

/*Send int16_t messages with different control values
Author: Alessio*/
void send_int_message(char control,int16_t value) {
	PacketData p;
	p.as_uint16_t = value;
	send_message(control,p);
}


#endif
