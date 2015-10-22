#include "fifo.h"
#include "types.h"
#include <stdio.h>

int main() {
	Fifo fifo;
	fifo_init(&fifo);

	printf("Size (0): %i\n", fifo_size(&fifo));

	char c;

	// put a char
	fifo_put(&fifo, 'a');
	printf("%i %i\n",fifo.in, fifo.out);
	printf("Size (1): %i\n", fifo_size(&fifo));
	fifo_pop(&fifo, &c);
	printf("Pop a: %c\n",c);

	// Pop when zero elements
	printf("Pop (0,a): %i, %c\n", fifo_pop(&fifo, &c), c);

	// Print the size
	fifo_put(&fifo, 'a');
	printf("Size (1): %i\n", fifo_size(&fifo));
	fifo_pop(&fifo, &c);

	// put several chars
	fifo_put(&fifo, 'a');
	fifo_put(&fifo, 'b');
	fifo_put(&fifo, 'c');
	printf("Size (3): %i\n", fifo_size(&fifo));
	fifo_pop(&fifo, &c);
	printf("Pop a: %c\n",c);
	fifo_pop(&fifo, &c);
	printf("Pop b: %c\n",c);
	fifo_pop(&fifo, &c);
	printf("Pop c: %c\n",c);

	// Print int 1
	fifo_put(&fifo, 1);
	fifo_pop(&fifo, &c);
	printf("Pop 1: %i\n",c);

	// put negative number
	fifo_put(&fifo, -127);
	fifo_pop(&fifo, &c);
	printf("Pop -127: %i\n",c);

	// Check peek
	fifo_put(&fifo, 34);
	fifo_peek(&fifo, &c);
	printf("Peek 34: %i\n",c);
	printf("Size 1: %i\n", fifo_size(&fifo));
	fifo_pop(&fifo,&c);

	// Check peek 2
	fifo_put(&fifo, 75);
	fifo_put(&fifo, 32);
	fifo_peek_at(&fifo, &c, 1);
	printf("Peek 32: %i\n",c);
	printf("Size 2: %i\n", fifo_size(&fifo));

	// overflowtest (does not add any more elements)
	printf("Size (2): %i\n", fifo_size(&fifo));

	size_t j;
	for(j = 0; j < 130; j++) {
		//printf("Size: %i (%i,%i)\n", fifo_size(&fifo), fifo.in, fifo.out);
		fifo_put(&fifo, j);
	}

	printf("Size (127): %i\n", fifo_size(&fifo));

	//printf("in: %i, out: %i\n", fifo.in, fifo.out);
	fifo_pop(&fifo, &c);
	printf("Pop 75: %i\n", c);
	fifo_pop(&fifo, &c);
	printf("Pop 32: %i\n", c);

	printf("Size (125): %i\n", fifo_size(&fifo));

	// Second FIFO
	Fifo fifo2;
	fifo_init(&fifo);
	fifo_put(&fifo2, 'b');
	printf("Size: %i\n", fifo_size(&fifo2));
 	fifo_pop(&fifo2, &c);
	printf("Pop b: %c\n",c);

	return 1;
}
