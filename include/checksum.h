#include "types.h"
#define PACKET_LENGTH 2

/* Perform an 8-bits checksum
   Author: Alessio */
//unsigned char checksum(char * data, int length)
//{
	/*With 8-bits checksum there's always the issue of being insensitive to the order of the bytes.
	This happens for example if we only do a sum of the bytes.
	The Fletcher checksum should avoid this problem */

	//unsigned char sum1, sum2 = 0;
	//int i;

	/* Fletcher checksum */
	/*for(i=0; i < length; i++)
	{
		sum1 = (sum1 + data[i]) % 255;
		sum2 = (sum2 + sum1)    % 255;
	}*/

	/*In addition we can add this line to shift the sum2 value. If the bytes order is wrong, the shift will be different
	and we can detect that the order is wrong.*/
	//return sum2 << data[0];


	//just the simplest checksum evah
	/*unsigned char sum;
	int i;
	for(i=0; i < length; i++)
	{
	sum+=*(data++);
	}
	  return sum;*/
//	return 'a';
//}

/* Generates a checksum based on a control variable and a value
 * Author: Alessio
 */
unsigned char packet_checksum(char control, char value) {
	return control ^ value;
}

/*  Check if the checksums match and return a boolean value
    Author: Alessio
*/
bool check_packet(char control, char value, unsigned char in_checksum) {
	return packet_checksum(control, value) == in_checksum;
}
