#include "types.h"
#include <stdio.h>
#define PACKET_LENGTH 3

/* Perform an 8-bits checksum
   Author: Alessio */

unsigned char checksum(char control, PacketData packet_data)
{
	/*With 8-bits checksum there's always the issue of being insensitive to the order of the bytes.
	This happens for example if we only compute a sum of the bytes.
	The Fletcher checksum should avoid this problem */

	unsigned char sum1,sum2 ;
	unsigned char data[3] = {0};
  	int i;

	data[0] = control;
  	data[1] = packet_data.as_bytes[0];
	data[2] = packet_data.as_bytes[1];

	sum1 = sum2 = 0;

	/* Fletcher checksum */
	for(i=0; i < PACKET_LENGTH; i++)
	{
		sum1 = (sum1 + data[i]) % 127;
		sum2 = (sum2 + sum1)    % 127;
	}

	/*In addition we can add this line to the sum2 value. If the bytes order is wrong, the XOR will produce
	a different output	and we can detect that the order is wrong.*/
	return sum2 ^ data[0];
}

/*  Check if the checksums match and return a boolean value
    Author: Alessio
*/
bool check_packet(char control, PacketData data, unsigned char in_checksum)
{
	return checksum(control, data) == in_checksum;
}
