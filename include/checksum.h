#include "types.h"
#include <stdio.h>
#define PAYLOAD_LENGTH 2

/*With 8-bits checksum there's always the issue of being insensitive to the order of the bytes.
This happens for example if we only compute a sum of the bytes. Another issue is that of the small
universe of cks values that a simple checksum provides.
The Fletcher checksum should avoid this problem*/

/* Efficient implementation of an 8-bits Fletcher checksum
   Author: Alessio */
uint8_t checksum(char control, PacketData packet_data)
{
	uint8_t sum1 = 0xFF, sum2 = 0xFF;
	uint8_t data[3] = {0};
  size_t i;

  /*first sum on the control value*/
	sum1 = ( (sum1 + control) ) & 0xFF + (sum1 >> 8);
	sum2 = ( (sum2 + sum1) ) & 0xFF + (sum2 >> 8);

	/*Loop on the payload.  The shifts are way more efficient than division,
	  and guarantee reduces the sums modulo 255*/
	for(i=0; i < PAYLOAD_LENGTH; i++)
	{
		sum2 += sum1 += packet_data.as_bytes[i];

		sum1 = (sum1 & 0xFF) + (sum1 >> 8);
		sum2 = (sum2  & 0xFF) + (sum2 >> 8);
	}

  /*after the previous calculations are in the range 1-0x1FE and thus an overflow is present.
	Therefore here we do another step to reduce the sums to 8 bits*/
	sum1 = (sum1 & 0xFF) + (sum1 >> 8);
	sum2 = (sum2  & 0xFF) + (sum2 >> 8);

	/*Sensitivity to the order of blocks is already introduced with the repeated ordered sum of each block in the sum2, after it was already added to sum1. In addition we can add this line to the sum2 value. If the bytes order is wrong, the XOR will produce
	a different output	and we can detect the error.*/
	return sum2 ^ sum1;
}

/*  Checks if the checksums match and return a boolean value
    Author: Alessio*/
bool check_packet(char control, PacketData data, unsigned char in_checksum)
{
	return checksum(control, data) == in_checksum;
}
