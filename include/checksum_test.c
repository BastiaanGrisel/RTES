#include "checksum.h"
#include <stdio.h>

int main(void) {
	char control[] = {'M','M','M','C','L','L','r','r','Z'};
	char value[] = {1,2,222,'f','e',127,'e',0xF,17};


  size_t i,j;
	for(i=0; i < 9; i++)
	{
			printf("Checksum %i %i = %i\n",control[i],value[i],checksum(control[i],ch2pd(value[i])));
	}

/*data[0] = 'M';
	data[1] = '5';
	control = 'M';
	value = '5';
	printf("Checksum M5(chars): %i\n", checksum(data,2));

	control = 'M';
	int value2 = 5;
	// Int will get chopped up, do not just put an int into this function. This is just for testing purposes.
	printf("Checksum M5(int): %i\n", checksum(control,value2));*/

	return 0;
}
