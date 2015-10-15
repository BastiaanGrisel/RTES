/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include "types.h"
#include "QRmessage.h"
#include "control.h"


#define LOG_SIZE 100
#define EVENT_SIZE 1000
extern Loglevel log_level = SENSORS;
int log_counter = 0;
int event_counter  = 0;

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
void init_log_arrays(int16_t tm_log[][3], int16_t sensor_log[][10],int16_t control_log[][11],int16_t event_array[][4]) {
  memset(tm_log,0,sizeof(tm_log[0][0]) *LOG_SIZE * 3); //memset functions is inlined by the compiler, so it's faster
  memset(sensor_log,0,sizeof(sensor_log[0][0]) *LOG_SIZE * 10);
  memset(control_log,0,sizeof(control_log[0][0]) *LOG_SIZE * 11);
  memset(event_array,0,sizeof(event_array[0][0]) *LOG_SIZE * 8);
}

/*Just for testing*/
void init_array_test(int16_t tm_log[][3],int16_t sensor_log[][10])
{
  	int i;

  	for(i=0; i < LOG_SIZE; i++)
    {

       tm_log[i][0] = 10000;
       tm_log[i][1] = 400;
       tm_log[i][2] = 24400;

   		sensor_log[i][0] = 1;
   		sensor_log[i][1] = 65400;
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
void log_tm(int16_t tm_log[][3], int16_t timestamp, int16_t mode)
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
void log_control(Mode mode, int16_t control_log[][11],int16_t s_bias[], int16_t R_stablize,
  int16_t P_stabilize, int16_t Y_stabilize,int16_t R_angle, int16_t P_angle) {
    if(log_counter < LOG_SIZE) {
      if(mode >= CALIBRATE) // better == ?
      {
       control_log[log_counter][0] = s_bias[0];
       control_log[log_counter][1] = s_bias[1];
       control_log[log_counter][2] = s_bias[2];
       control_log[log_counter][3] = s_bias[3];
       control_log[log_counter][4] = s_bias[4];
       control_log[log_counter][5] = s_bias[5];

       //downcasting control values, right shift to keep the significant bits
       control_log[log_counter][8] = RSHIFT(Y_stabilize,4);
       control_log[log_counter][6] = RSHIFT(R_stablize,4);
       control_log[log_counter][7] = RSHIFT(P_stabilize,4);
       control_log[log_counter][9] = RSHIFT(R_angle,4);
       control_log[log_counter][10] = RSHIFT(P_angle,4);
      }
  }
}



//sensors and rpm
void log_sensors(int16_t sensor_log[][10], int16_t timestamp, int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5,
  int16_t rpm0, int16_t rpm1, int16_t rpm2, int16_t rpm3) {

	//if(log_counter < LOG_SIZE) {} else {log_counter--;}
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

}


void log_event(int16_t event_array[][4],int32_t timestamp, char control, int16_t value)
{

     event_array[event_counter][0] = timestamp & 0x0000FFFF; //low
     event_array[event_counter][1] = timestamp >> 16; //high
     event_array[event_counter][2] = control;
     event_array[event_counter][3] = value;

  if(event_counter++ == LOG_EVENT){
    send_err_message(SENSOR_LOG_FULL);
  }
}



void clear_log() {
	log_counter = 0;
}

/*Send all the logs relative sensors and control chain  */
void send_logs(int16_t tm_log[][3], int16_t sensor_log[][10], int16_t control_log[][11]) {
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
      p.as_uint16_t = control_log[i][j];
      send_message(LOG_MSG_PART, p);
    //send_highlow(control_log[i][j]);
    }

		send_control_message(LOG_MSG_NEW_LINE);

		if(i%(LOG_SIZE/100)==(LOG_SIZE/100)-1){
			sprintf(message, "%i%%",i/100+1);
			send_term_message(message);
		}

	}//for i

	send_term_message("LOGGING COMPLETED.");
}


void send_logs_event(int16_t event_array[][4])
{
   int i,j;
   PacketData p;

   for(i=0; i < EVENT_SIZE; i++)
   {
      for(j=0; j < 3; j++){
         p.as_uint16_t = event_array[i][j];
         send_message(LOG_EVENT, p);
       }

       send_message(LOG_EV_NEW_LINE,p);
   }//for

}

#endif
