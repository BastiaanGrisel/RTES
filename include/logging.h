/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include "QRmessage.h"


#define LOG_SIZE 20
sensor_log_counter = 0;

extern char message[100];

/*Just for testing*/
void init_array(unsigned int sensor_log[][7])
{
  	int i;
  	for(i=0; i < LOG_SIZE; i++)
    {
   		sensor_log[i][0] = 1;
   		sensor_log[i][1] = 500;
   		sensor_log[i][2] = 255;
   		sensor_log[i][3] = 500;
   		sensor_log[i][4] = 65101;
   		sensor_log[i][5] = 65540;
   		sensor_log[i][6] = 65499;
   }
}

void log_sensors(unsigned int sensor_log[][7], int32_t timestamp, int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5) {
	if(sensor_log_counter < LOG_SIZE) {
		sensor_log[sensor_log_counter][0] = timestamp;
		sensor_log[sensor_log_counter][1] = s0;
		sensor_log[sensor_log_counter][2] = s1;
		sensor_log[sensor_log_counter][3] = s2;
		sensor_log[sensor_log_counter][4] = s3;
		sensor_log[sensor_log_counter][5] = s4;
		sensor_log[sensor_log_counter][6] = s5;

		if(sensor_log_counter++ == LOG_SIZE){
			send_err_message(SENSOR_LOG_FULL);
		}
	}
}

void clear_log() {
	sensor_log_counter = 0;
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

			send_message(LOG_MSG_PART,p);

      //OLD WAY
			/*  unsigned char low  =  sensor_log[i][j]       & 0xff;
      unsigned char high = (sensor_log[i][j] >> 8) & 0xff;
      send_message(LOG_MSG_PART, ch2pd(high));
			send_message(LOG_MSG_PART, ch2pd(low));*/
		}

		send_control_message(LOG_MSG_NEW_LINE);

		if(i%(LOG_SIZE/100)==(LOG_SIZE/100)-1){
			sprintf(message, "%i%%",i/100+1);
			send_term_message(message);
		}
	}
	send_term_message("LOGGING COMPLETED.");
}



#endif
