#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "types.h"
#include "fifo.h"

void check_for_new_packets(Fifo *q, void (*callback)(char, PacketData), void (*error)()){
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

			error();
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

#endif
