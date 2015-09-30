/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include "communication_qr2pc.h"

#define LOG_SIZE 20

extern char message[100];

/*Just for testing*/
void add_dummy_data(unsigned int sensor_log[][7])
{
  	int i;
  	for(i=0; i < LOG_SIZE; i++)
    {
   		sensor_log[i][0] = 1;
   		sensor_log[i][1] = 500;
   		sensor_log[i][2] = 255;
   		sensor_log[i][3] = 500;
   		sensor_log[i][4] = 100;
   		sensor_log[i][5] = 200;
   		sensor_log[i][6] = 0;
   }
}

/* Send over the logs that are stored in 'sensor_log'
 */
void send_logs(unsigned int sensor_log[][7]) {
	int i;
	int j;
	PacketData p;

	for(i = 0; i < LOG_SIZE; i++) {
		for(j = 0; j < 7; j++) {

			//PROVISIONAL workaround
			p.as_uint16_t = (sensor_log[i][j] == 255) ? 32000 : sensor_log[i][j];

			send_packet(rs232_putchar, LOG_MSG_PART, p);

      //OLD WAY
			/*  unsigned char low  =  sensor_log[i][j]       & 0xff;
      unsigned char high = (sensor_log[i][j] >> 8) & 0xff;
      send_message(LOG_MSG_PART, ch2pd(high));
			send_message(LOG_MSG_PART, ch2pd(low));*/
		}

		send_control(rs232_putchar, LOG_MSG_NEW_LINE);

		if(i%(LOG_SIZE/100)==(LOG_SIZE/100)-1){
			sprintf(message, "%i%%",i/100+1);
			send_term_message(message);
		}
	}
	send_term_message("LOGGING COMPLETED");
}



#endif
