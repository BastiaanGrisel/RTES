#include <stdio.h>
#include "types.h"

#define FIFO_CAPACITY 127

typedef struct {
	int in;
	int out;
	char elements[127];
} Fifo;


#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
int fifo_in, fifo_out;

void fifo_init(Fifo *fifo)
{
	fifo->in = 0;
	fifo->out = 0;
}

// Add char to queue
bool fifo_put(Fifo* fifo, char c)
{
	if(fifo_size() == FIFO_CAPACITY)
		return false;

	fifo->elements[fifo->in++] = c;
	return true;
}

// pop
int fifo_get(Fifo* fifo, char *old, bool remove, int n)
{
	if(fifo_size() == 0)
		return false;

	*old = fifo->elements[((fifo->out + n) % FIFO_CAPACITY)];

	if(remove)
		fifo->out++;

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
	else
		return FIFO_CAPACITY + fifo->in - fifo->out;
}
