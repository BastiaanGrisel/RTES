#ifndef PCMESSAGE_H
#define PCMESSAGE_H

#include "communication.h"
#include "qrio.h"
#include "times.h"
#include "fifo.h"

/**************** PC -> QR ********************/
/* send a complete message PC -> QR
 * Author: Henko Aantjes
 */
void pc_send_message(char control, char value){
	PacketData p;
	p.as_char = value;

	rs232_putchar(control);
	rs232_putchar(p.as_bytes[0]);
	rs232_putchar(p.as_bytes[1]);
	rs232_putchar(checksum(control,p));
	update_time();
}


#endif
