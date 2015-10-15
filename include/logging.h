/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include "types.h"
#include "QRmessage.h"


#define LOG_SIZE 20
extern Loglevel log_level = SENSORS;
log_counter = 0;

extern char message[100] = {0};

/*Standard function to send 32bit values in 2 different 16bits packets*/
void send_highlow(int32_t val32)
{
  PacketData p1,p2;
  p1.as_uint16_t = val32 & 0x0000FFFF; //low
  p2.as_uint16_t = val32 >> 16; //high

  send_message(LOG_MSG_PART,p1), send_message(LOG_MSG_PART,p2);
}


/*Zeroing all the log arrays. */
void init_log_arrays(int32_t tm_log[][3], int32_t sensor_log[][10],int32_t control_log[][11],int16_t keyjs_log[][8]) {
  memset(tm_log,0,sizeof(tm_log[0][0]) *LOG_SIZE * 3); //memset functions is inlined by the compiler, so it's faster
  memset(sensor_log,0,sizeof(sensor_log[0][0]) *LOG_SIZE * 10);
  memset(control_log,0,sizeof(control_log[0][0]) *LOG_SIZE * 10);
  memset(keyjs_log,0,sizeof(keyjs_log[0][0]) *LOG_SIZE * 10);
}

/*Just for testing*/
void init_array_test(int32_t tm_log[][3],int32_t sensor_log[][10])
{
  	int i;

  	for(i=0; i < LOG_SIZE; i++)
    {

       tm_log[i][0] = 100000;
       tm_log[i][1] = 400;
       tm_log[i][2] = 24400;

   		sensor_log[i][0] = 1;
   		sensor_log[i][1] = 65500;
   		sensor_log[i][2] = 255;
   		sensor_log[i][3] = 500;
   		sensor_log[i][4] = 65101;
   		sensor_log[i][5] = 400;
   		sensor_log[i][6] = 0;
      sensor_log[i][7] = 0;
      sensor_log[i][8] = 0;
      sensor_log[i][9] = 0;
   }
}

//TM = time & mode
void log_tm(int32_t tm_log[][3], int32_t timestamp, int16_t mode)
{
  if(log_counter < LOG_SIZE) {
     tm_log[log_counter][0] = timestamp & 0x0000FFFF;
     tm_log[log_counter][1] = timestamp >> 16;
     tm_log[log_counter][2] = mode;
  }

  if(log_counter++ == LOG_SIZE){
    send_err_message(SENSOR_LOG_FULL);
  }
}


//5sensors bias - RPYstablize -RP Also once: the bias, which are available after calibration
void log_control(Mode mode, int32_t control_log[][11],int32_t s_bias[], int32_t R_stablize,
  int32_t P_stabilize, int32_t Y_stabilize,int32_t R_angle, int32_t P_angle) {
    if(log_counter < LOG_SIZE) {
      if(mode >= CALIBRATE) // better == ?
      {
       control_log[log_counter][0] = s_bias[0];
       control_log[log_counter][1] = s_bias[1];
       control_log[log_counter][2] = s_bias[2];
       control_log[log_counter][3] = s_bias[3];
       control_log[log_counter][4] = s_bias[4];
       control_log[log_counter][5] = s_bias[5];

       if(mode >= YAW_CONTROL)
         {
           control_log[log_counter][8] = Y_stabilize;
              if(mode == FULL_CONTROL)
              {
                control_log[log_counter][6] = R_stablize;
                control_log[log_counter][7] = P_stabilize;
                control_log[log_counter][9] = R_angle;
                control_log[log_counter][10] = P_angle;
              }
         }
      }

  }

    if(log_counter++ == LOG_SIZE){
      send_err_message(SENSOR_LOG_FULL);
    }
}



//sensors and rpm
void log_sensors(int32_t sensor_log[][10], int32_t timestamp, int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5,
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

void log_keyjs(int16_t keyjs_log[][8],int16_t current_values[])
{
  int i;
  if(log_counter < LOG_SIZE) {
     for(i=0; i < 8; i++)
       keyjs_log[log_counter][i] = current_values[i];
  }

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
void send_logs(int32_t tm_log[][3], int32_t sensor_log[][10], int32_t control_log[][11], int16_t keyjs_log[][8]) {
	int i;
	int j;
	PacketData p;
	/*p.as_uint16_t = 255;
	send_message(LOG_MSG_PART, p);*/

  for(i=0; i < LOG_SIZE; i++)
  {
    for(j=0; j < 3; j++){
      p.as_uint16_t = tm_log[i][j];
      send_message(LOG_MSG_PART, p);
    }

		for(j = 0; j < 10; j++) {
			p.as_uint16_t = sensor_log[i][j];
			send_message(LOG_MSG_PART, p);
		}

    for(j = 0; j < 11; j++) {
    //  p.as_uint16_t = control_log[i][j];
    //  send_message(LOG_MSG_PART, p);
    send_highlow(control_log[i][j]);
    }

  /*  for(j=0; j < 8; j++){
      p.as_uint16_t = keyjs_log[i][j];
			send_message(LOG_MSG_PART, p);
    }*/

		send_control_message(LOG_MSG_NEW_LINE);

		if(i%(LOG_SIZE/100)==(LOG_SIZE/100)-1){
			sprintf(message, "%i%%",i/100+1);
			send_term_message(message);
		}
	}
	send_term_message("LOGGING COMPLETED.");
}

#endif
