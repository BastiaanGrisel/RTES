#include "fifo.h"
#include <stdio.h>

int main() {
	fifo_init();
	char c;
	int i;	
	
	// put a char
	fifo_put('a');
	fifo_get(&c);
	printf("Pop a: %c\n",c);

	// Print the size
	fifo_put('a');
	printf("Size (1): %i\n", fifo_size()); 
	fifo_get(&c);
	
	// put several chars
	fifo_put('a');
	fifo_put('b');
	fifo_put('c');
	printf("Size (3): %i\n", fifo_size());
	fifo_get(&c);
	printf("Pop a: %c\n",c);
	fifo_get(&c);
	printf("Pop b: %c\n",c);
	fifo_get(&c);
	printf("Pop b: %c\n",c);

	// Print int 1
	fifo_put(1);
	fifo_get(&c);
	printf("Pop 1: %i\n",c);

	// put negative number
	fifo_put(-127);
	fifo_get(&c);
	printf("Pop -127: %i\n",c);
	
	// overflowtest (does not add any elements)
	int j;
	for(j = 0; j < 110; j++) {
		fifo_put(j);
	}	

	fifo_print();

	printf("Size (100): %i\n", fifo_size());
	


	// Get the size	

	
	return 1;
}





