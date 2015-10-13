/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include "QRmessage.h"


#define LOG_SIZE 20
extern Loglevel log_level = SENSORS;
log_counter = 0;

extern char message[100] = {0};


/*Zeroing all the log arrays. */
void init_log_arrays(unsigned int tm_log[][3], unsigned int sensor_log[][10]) {
  memset(tm_log,0,sizeof(tm_log[0][0]) *LOG_SIZE * 3); //memset functions is inlined by the compiler, so it's faster
  memset(sensor_log,0,sizeof(sensor_log[0][0]) *LOG_SIZE * 10);
}

/*Just for testing*/
void init_array_test(unsigned int sensor_log[][10])
{
  	int i;
  	for(i=0; i < LOG_SIZE; i++)
    {
   		sensor_log[i][0] = 1;
   		sensor_log[i][1] = 65500;
   		sensor_log[i][2] = 255;
   		sensor_log[i][3] = 500;
   		sensor_log[i][4] = 65101;
   		sensor_log[i][5] = 400;
   		sensor_log[i][6] = 0;
   }
}

//TM = time & mode
void log_tm(unsigned int tm_log[][3], int32_t timestamp, int16_t mode)
{
  if(log_counter < LOG_SIZE) {
     tm_log[log_counter][0] = timestamp & 0x0000FFFF;
     tm_log[log_counter][1] = (timestamp >> 16);
     tm_log[log_counter][2] = mode;
  }

  if(log_counter++ == LOG_SIZE){
    send_err_message(SENSOR_LOG_FULL);
  }
}

//TODO
//void log_control(unsigned int control_log[][3],int32_t Y_stabilize, int32_t P_stabilize, ...)

//sensors and rpm
void log_sensors(unsigned int sensor_log[][10], int32_t timestamp, int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5,
  int32_t rpm0, int32_t rpm1, int32_t rpm2, int32_t rpm3) {

	if(log_counter < LOG_SIZE) {} else {log_counter--;}
	sensor_log[log_counter][0] = s0;
	sensor_log[log_counter][1] = s1;
	sensor_log[log_counter][2] = s2;
	sensor_log[log_counter][3] = s3;
	sensor_log[log_counter][4] = s4;
	sensor_log[log_counter][5] = s5;

  sensor_log[log_counter][6] = rpm0;
  sensor_log[log_counter][7] = rpm1;
  sensor_log[log_counter][8] = rpm2;
  sensor_log[log_counter][9] = rpm3;


	if(log_counter++ == LOG_SIZE){
		send_err_message(SENSOR_LOG_FULL);
	}
}



void clear_log() {
	log_counter = 0;
}

/* TODO Send over the logs that are stored in 'sensor_log'
Here we need a case function like:
case YAW: log also the YAW chain
case FULL: log all the full chain
default: in every other mode logs only sensors and rpm.
In order to do this we need a variable that keeps trace of which was the mode in the moment of acquiring the logs
 */
void send_logs(unsigned int sensor_log[][10]) {
	int i;
	int j;
	PacketData p;
	/*p.as_uint16_t = 255;
	send_message(LOG_MSG_PART, p);*/

	for(i = 0; i < LOG_SIZE; i++) {
		for(j = 0; j < 7; j++) {
			p.as_uint16_t = sensor_log[i][j];

			send_message(LOG_MSG_PART, p);
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
