/* Author Bastiaan */

#ifndef FIFO_H
#define FIFO_H

#include <stdio.h>
#include "types.h"

#define FIFO_CAPACITY 256 // Can store 127 values!

/* Structure to hold fifo so multiple ones can be made */
typedef struct {
	int in;
	int out;
	char elements[256];
} Fifo;

/* Call this function beofre using the fifo */
void fifo_init(Fifo *fifo)
{
	fifo->in = 0;
	fifo->out = 0;
}

/* Append a character to the fifo as long as it is not full */
bool fifo_put(Fifo* fifo, char c)
{
	if(fifo_size(fifo) == FIFO_CAPACITY - 1)
		return false;

	fifo->elements[(fifo->in)++] = c;
	fifo->in %= FIFO_CAPACITY;

	return true;
}

/* Get a character from the fifo at position n */
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

/* Get the first character in the fifo (without removing it) */
bool fifo_peek(Fifo* fifo, char *c) {
	return fifo_get(fifo, c, false, 0);
}

/* Get the n-th character of the fifo (without removing it) */
bool fifo_peek_at(Fifo* fifo, char *c, int n) {
	return fifo_get(fifo, c, false, n);
}

/* Remove the first character in the fifo */
bool fifo_pop(Fifo* fifo, char *c) {
	return fifo_get(fifo, c, true, 0);
}

/* Get the size of the fifo */
int fifo_size(Fifo* fifo) {
	if(fifo->in > fifo->out)
		return fifo->in - fifo->out;
	else if(fifo->in == fifo->out)
		return 0;
	else
		return FIFO_CAPACITY + fifo->in - fifo->out;
}

#endif
