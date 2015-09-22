#include <stdio.h>

#define QUEUE_ELEMENTS 100
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
char fifo_[QUEUE_SIZE];
int fifo_in, fifo_out;

void fifo_init(void)
{
    fifo_in = fifo_out = 0;
}

int fifo_put(char c)
{
    if(fifo_in == (( fifo_out - 1 + QUEUE_SIZE) % QUEUE_SIZE)) {
    	return -1; /* fifo_ Full*/
    }

    fifo_[fifo_in] = c;

    fifo_in = (fifo_in + 1) % QUEUE_SIZE;

    return 0; // No errors
}

char fifo_get(char *old)
{
    if(fifo_in == fifo_out) {
        return -1; /* fifo_ Empty - nothing to get*/
    }

    *old = fifo_[fifo_out];

    fifo_out = (fifo_out + 1) % QUEUE_SIZE;

    return 0; // No errors
}

int fifo_size() {
	if(fifo_in > fifo_out)
		return fifo_in - fifo_out;
	else
		return QUEUE_SIZE + fifo_in - fifo_out;
}

void fifo_print() {
	int i = 0;
	for(i = 0; i < QUEUE_ELEMENTS; i++) {
		if(i == fifo_in)		
			printf("(in) ");
		if(i == fifo_out)
			printf("(out) ");

		printf("%i\n", fifo_[i]);
	}
}
