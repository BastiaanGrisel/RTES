#include <stdio.h>
#include "types.h"

#define QUEUE_ELEMENTS 100
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
char fifo_[QUEUE_ELEMENTS];
int fifo_in, fifo_out;

void fifo_init(void)
{
	fifo_in = fifo_out = 0;
}

// Add char to queue
bool fifo_put(char c)
{
	if(fifo_size() == QUEUE_ELEMENTS)
		return false;

	fifo_[fifo_in++] = c;
	return true;
}

// pop
int fifo_get(char *old, bool remove, int n)
{
	if(fifo_size() == 0)
		return false;

	*old = fifo_[((fifo_out + n) % QUEUE_ELEMENTS)];

	if(remove)
		fifo_out++;

	return true;
}

// peek at first char
char fifo_peek() {
	char c;
	fifo_get(&c, false, 0);
	return c;
}

char fifo_peek_at(int n) {
	char c;
	fifo_get(&c, false, n);
	return c;
}

// pop first char
char fifo_pop() {
	char c;
	fifo_get(&c, true, 0);
	return c;
}

int fifo_size() {
	if(fifo_in > fifo_out)
		return fifo_in - fifo_out;
	else
		return QUEUE_SIZE + fifo_in - fifo_out;
}
