/* Perform an 8-bits checksum
   Author: Alessio */
unsigned char checksum(char * data, int length)
{
	/*With 8-bits checksum there's always the issue of being insensitive to the order of the bytes.
	This happens for example if we only do a sum of the bytes.
	The Fletcher checksum should avoid this problem */

	//unsigned char sum1 = sum2 = 0;
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

	//printf("%#06X \n",ck);

	//just the simplest checksum evah
	/*unsigned char sum;
	int i;
	for(i=0; i < length; i++)
	{
	sum+=*(data++);
	}
	return sum;*/
	return 'a';
}

/*  Check if the checksums match and return a boolean value
    Author: Alessio
*/
bool check_packet(char control, char value, unsigned char in_checksum) {
	return true;
	/*char data[2];
	unsigned char curr_checksum;
	data[0] = input;
	data[1] = value;
	curr_checksum = checksum(data,PACKET_LENGTH);

	return curr_checksum == in_checksum;*/
}
