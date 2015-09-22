#include <stdio.h>

#define QUEUE_ELEMENTS 100
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
char fifo_[QUEUE_SIZE];
int fifo_in, fifo_Out;

void fifo_init(void)
{
    fifo_in = fifo_Out = 0;
}

int fifo_put(char c)
{
    if(fifo_in == (( fifo_Out - 1 + QUEUE_SIZE) % QUEUE_SIZE))
    {
        return -1; /* fifo_ Full*/
    }

    fifo_[fifo_in] = c;

    fifo_in = (fifo_in + 1) % QUEUE_SIZE;

    return 0; // No errors
}

char fifo_get(char *old)
{
    if(fifo_in == fifo_Out)
    {
        return -1; /* fifo_ Empty - nothing to get*/
    }

    *old = fifo_[fifo_Out];

    fifo_Out = (fifo_Out + 1) % QUEUE_SIZE;

    return 0; // No errors
}

void fifo_print() {
	
	int i = 0;
	for(i = 0; i < QUEUE_ELEMENTS; i++) {
		if(i == fifo_in)		
			printf("(in) ");
		if(i == fifo_Out)
			printf("(out) ");

		printf("%i\n", fifo_[i]);
	}

}
