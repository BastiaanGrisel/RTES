#include "fifo.h"
#include "types.h"
#include <stdio.h>

int main() {
	Fifo fifo;
	fifo_init(&fifo);

	printf("Size (0): %i\n", fifo_size(&fifo)); 
	
	// put a char
	fifo_put(&fifo, 'a');
	printf("%i %i\n",fifo.in, fifo.out);
	printf("Size (1): %i\n", fifo_size(&fifo)); 
	printf("Pop a: %c\n",fifo_pop(&fifo));

	// Print the size
	fifo_put(&fifo, 'a');
	printf("Size (1): %i\n", fifo_size(&fifo)); 
	fifo_pop(&fifo);
	
	// put several chars
	fifo_put(&fifo, 'a');
	fifo_put(&fifo, 'b');
	fifo_put(&fifo, 'c');
	printf("Size (3): %i\n", fifo_size(&fifo));
	printf("Pop a: %c\n",fifo_pop(&fifo));
	printf("Pop b: %c\n",fifo_pop(&fifo));
	printf("Pop c: %c\n",fifo_pop(&fifo));
	
	// Print int 1
	fifo_put(&fifo, 1);
	printf("Pop 1: %i\n",fifo_pop(&fifo));

	// put negative number
	fifo_put(&fifo, -127);
	printf("Pop -127: %i\n",fifo_pop(&fifo));
	
	// Check peek
	fifo_put(&fifo, 34);
	printf("Peek 34: %i\n",fifo_peek(&fifo));
	printf("Size 1: %i\n", fifo_size(&fifo));
	fifo_pop(&fifo);
	
	// Check peek 2
	fifo_put(&fifo, 75);
	fifo_put(&fifo, 32);
	printf("Peek 32: %i\n",fifo_peek_at(&fifo, 1));
	printf("Size 2: %i\n", fifo_size(&fifo));

	// overflowtest (does not add any elements)
	printf("Size (2): %i\n", fifo_size(&fifo));

	int j;
	for(j = 0; j < 130; j++) {
		//printf("Size: %i (%i,%i)\n", fifo_size(&fifo), fifo.in, fifo.out);
		fifo_put(&fifo, j);
	}	

	printf("Size (127): %i\n", fifo_size(&fifo));
	printf("in: %i, out: %i\n", fifo.in, fifo.out);
	printf("Pop 75: %i\n", fifo_pop(&fifo));	
	printf("Pop 32: %i\n", fifo_pop(&fifo));	

	// Second FIFO
	Fifo fifo2;
	fifo_init(&fifo);
	fifo_put(&fifo2, 'b');

	return 1;
}





