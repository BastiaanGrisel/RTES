#include <stdio.h>
#include "types.h"

#define FIFO_CAPACITY 128 // Can store 127 values!

typedef struct {
	int in;
	int out;
	char elements[128];
} Fifo;

void fifo_init(Fifo *fifo)
{
	fifo->in = 0;
	fifo->out = 0;
}

// Add char to queue
bool fifo_put(Fifo* fifo, char c)
{
	if(fifo_size(fifo) == FIFO_CAPACITY - 1)
		return false;

	fifo->elements[(fifo->in)++] = c;
	fifo->in %= FIFO_CAPACITY;

	return true;
}

// pop
int fifo_get(Fifo* fifo, char *old, bool remove, int n)
{
	if(fifo_size(fifo) == 0)
		return false;

	*old = fifo->elements[((fifo->out + n) % FIFO_CAPACITY)];

	if(remove)
		fifo->out++;

	fifo->out %= FIFO_CAPACITY;

	return true;
}

// peek at first char
char fifo_peek(Fifo* fifo) {
	char c;
	fifo_get(fifo, &c, false, 0);
	return c;
}

char fifo_peek_at(Fifo* fifo, int n) {
	char c;
	fifo_get(fifo, &c, false, n);
	return c;
}

// pop first char
char fifo_pop(Fifo* fifo) {
	char c;
	fifo_get(fifo, &c, true, 0);
	return c;
}

int fifo_size(Fifo* fifo) {
	if(fifo->in > fifo->out)
		return fifo->in - fifo->out;
	else if(fifo->in == fifo->out)
		return 0;
	else
		return FIFO_CAPACITY + fifo->in - fifo->out;
}
