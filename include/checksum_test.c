#include "checksum.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
	int flag = 1;
	int size = (flag == 1 ) ? 40 : 10;
	int range = 256;
	srand(time(NULL));
  char control[size];
	char val1[size];
	char val2[size];
	/*char  control[] = {'M','M','M','C','L','L','r','r','Z'};
  char    val1[] = {1,2,-78,'z','x',125,'e',0x2C,17};
  char	  val2[] = {1,2,222,'f','e',127,'e',0x2C,100};*/

  size_t i,j;
	PacketData pd;

if(flag)
  for(i=0; i < size; i++)
	{
		control[i] = rand() % range +1;
	  val1[i] = rand() % range + 1;
		val2[i] = rand() % range + 1;
	}

	for(i=0; i < size ; i++)
	{
		  pd.as_bytes[0] = val1[i];
			pd.as_bytes[1] = val2[i];
			printf("Checksum %i %i %i= %i\n",control[i],val1[i],val2[i],checksum(control[i],pd));
	}

	return 0;
}
