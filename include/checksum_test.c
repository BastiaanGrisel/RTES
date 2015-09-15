#include "checksum.h"
#include <stdio.h>

int main() {
	char control = 'A';
	char value = 'f';

	printf("Checksum Af: %i\n", packet_checksum(control,value));

	control = 'M';
	value = '5';
	printf("Checksum M5(chars): %i\n", packet_checksum(control,value));

	control = 'M';
	int value2 = 5;
	// Int will get chopped up, do not just put an int into this function. This is just for testing purposes.
	printf("Checksum M5(int): %i\n", packet_checksum(control,value2)); 

	return 0;
}
