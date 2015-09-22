#include "fifo.h"
#include "types.h"
#include <stdio.h>

int main() {
	fifo_init();
	char c;
	int i;	
	
	// put a char
	fifo_put('a');
	printf("Pop a: %c\n",fifo_pop());

	// Print the size
	fifo_put('a');
	printf("Size (1): %i\n", fifo_size()); 
	fifo_pop();
	
	// put several chars
	fifo_put('a');
	fifo_put('b');
	fifo_put('c');
	printf("Size (3): %i\n", fifo_size());
	printf("Pop a: %c\n",fifo_pop());
	printf("Pop b: %c\n",fifo_pop());
	printf("Pop b: %c\n",fifo_pop());

	// Print int 1
	fifo_put(1);
	printf("Pop 1: %i\n",fifo_pop());

	// put negative number
	fifo_put(-127);
	printf("Pop -127: %i\n",fifo_pop());
	
	// Check peek
	fifo_put(34);
	printf("Peek 34: %i\n",fifo_peek());
	printf("Size 1: %i\n", fifo_size());
	fifo_pop();
	
	// Check peek 2
	fifo_put(75);
	fifo_put(32);
	printf("Peek 32: %i\n",fifo_peek_at(1));
	printf("Size 2: %i\n", fifo_size());

	// overflowtest (does not add any elements)
	int j;
	for(j = 0; j < 110; j++) {
		fifo_put(j);
	}	

	printf("Size (100): %i\n", fifo_size());
	printf("Pop 75: %i\n", fifo_pop());	
	printf("Pop 32: %i\n", fifo_pop());	

	return 1;
}





