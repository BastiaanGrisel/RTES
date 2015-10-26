/*Author: Alessio*/
#ifndef TIME_H
#define TIME_H

#include <sys/time.h>
#include <sys/types.h>
#include "PCmessage.h"

#define TIMEOUT 150 //ms

extern struct timeval keep_alive;
extern int ms_last_packet_sent;


/*Update the ms_last_packet_sent variable
Author: Alessio*/
void update_time()
{
	gettimeofday(&keep_alive,NULL);
	ms_last_packet_sent = ((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000);
}

#endif
