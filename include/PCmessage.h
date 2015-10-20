#ifndef PCMESSAGE_H
#define PCMESSAGE_H

#include "communication.h"
#include "qrio.h"
#include "times.h"
#include "fifo.h"

extern int out_packet_counter;
extern char last_out_message[4];
extern int fd_RS232;

/**************** PC -> QR ********************/
/* send a complete message PC -> QR
 * Author: Henko Aantjes
 */
void pc_send_message(char control, char value){
	if(fd_RS232 <= 0) return;
	PacketData p;
	p.as_char = value;

	rs232_putchar(control);
	rs232_putchar(p.as_bytes[0]);
	rs232_putchar(p.as_bytes[1]);
	rs232_putchar(checksum(control,p));

	update_time();
	out_packet_counter++;
	if(control != 0) sprintf(last_out_message, "%c%i (%i)", control, (int) value, checksum(control,ch2pd(value)));
}


#endif
