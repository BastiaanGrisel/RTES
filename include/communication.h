#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "types.h"
#include "fifo.h"
#include "checksum.h"

/* Check if a new packets have arrived
 * Author: Bastiaan
 */
void check_for_new_packets(Fifo *q, void (*callback)(char, PacketData)) {
	while(fifo_size(q) >= 4) { // Check if there are one or more packets in the queue
		char control;
		PacketData data;
		char checksum;

		fifo_peek_at(q, &control, 0);
		fifo_peek_at(q, &data.as_bytes[0], 1);
		fifo_peek_at(q, &data.as_bytes[1], 2);
		fifo_peek_at(q, &checksum, 3);

		if(!check_packet(control,data,checksum)) {
			// If the checksum is not correct, pop the first message off the queue and repeat the loop
			char c;
			fifo_pop(q, &c);
		} else {
			// If the checksum is correct, pop the packet off the queue and notify a callback
			char c;
			fifo_pop(q, &c);
			fifo_pop(q, &c);
			fifo_pop(q, &c);
			fifo_pop(q, &c);

			callback(control, data);
		}
	}
}

/* Send a complete packet
 * Author: Henko Aantjes
 */
void send_packet(void (*putchar)(char), char control, PacketData data) {
	putchar(control);
	putchar(data.as_bytes[0]);
	putchar(data.as_bytes[1]);
	putchar(checksum(control, data));
}

/* Send a sincle character
 * Author: Henko Aantjes
 */
void send_char(void (*putchar)(char), char control, char value) {
	PacketData p;
	p.as_char = value;
	send_packet(putchar, control, p);
}

/* Send a message that doesn't need a value
 * Author: Henko Aantjes
 */
void send_control(void (*putchar)(char), char control) {
	send_char(putchar, control, NOT_IMPORTANT);
}

/* Send an entire string at once
 * Author: Henko Aantjes
 */
void send_string(void (*putchar)(char), char control, char message[]) {
	int32_t i;
	for (i = 0; message[i] != 0; i++){
		send_char(putchar, control, message[i]);
	}
}

#endif
