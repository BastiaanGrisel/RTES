/*Author: Alessio*/
#ifndef LOG_H
#define LOG_H

#include <string.h>
#include "types.h"
#include "QRmessage.h"
#include "control.h"

#define CLOCK	peripherals[PERIPHERAL_US_CLOCK]

#define LOG_SIZE 10000
#define EVENT_SIZE 5

extern bool always_log;
extern bool log_data_completed;
extern bool log_ev_completed;

int32_t log_counter = 0;
int32_t event_counter  = 0;

extern char message[100] = {0};

/***utils logging functions***/
/*Standard function to send 32bit values in 2 different 16bits packets*/
void send_highlow(int32_t val32)
{
  PacketData p1,p2;
  p1.as_uint16_t = val32 & 0x0000FFFF; //low
  p2.as_uint16_t = val32 >> 16; //high
  send_message(LOG_MSG_PART,p1), send_message(LOG_MSG_PART,p2);
}


/*Zeroing all the log arrays. */
void init_log_arrays(int16_t tm_array[][3], int16_t sbias_array[], int16_t sensor_array[][10],int16_t control_array[][5],int16_t event_array[][4]) {
  log_counter = event_counter = 0;

  memset(tm_array,0,sizeof(tm_array[0][0]) *LOG_SIZE * 3); //memset functions is inlined by the compiler, so it's faster
  memset(sbias_array,0,sizeof(sbias_array[0]) *6);
  memset(sensor_array,0,sizeof(sensor_array[0][0]) *LOG_SIZE * 10);
  memset(control_array,0,sizeof(control_array[0][0]) *LOG_SIZE * 5);
  memset(event_array,0,sizeof(event_array[0][0]) *EVENT_SIZE * 4);
}


void clear_log() {
  always_log = false;
	log_counter = 0;
  event_counter = 0;
}

/*Just for testing*/
void init_array_test(int16_t tm_array[][3],int16_t sensor_array[][10], int16_t control_array[][5])
{
  	int i;

  	for(i=0; i < LOG_SIZE; i++)
    {

      tm_array[i][0] = CLOCK >> 15;
      tm_array[i][1] = CLOCK & 0x00007FFF;
      tm_array[i][2] = 2;

   		sensor_array[i][0] = 1;
   		sensor_array[i][1] = 600;
   		sensor_array[i][2] = 255;
   		sensor_array[i][3] = 500;
   		sensor_array[i][4] = 345;
   		sensor_array[i][5] = 400;
   		sensor_array[i][6] = 0;
      sensor_array[i][7] = 0;
      sensor_array[i][8] = 0;
      sensor_array[i][9] = 0;

      control_array[i][0] = 2;
      control_array[i][1] = 2;
      control_array[i][2] = 2;
      control_array[i][3] = 250;
      control_array[i][4] = 1000;
   }
}


/**** actual logging functions ****/
//TM = time & mode
void log_tm(int16_t tm_array[][3], int16_t timestamp, int16_t mode)
{
  tm_array[log_counter][0] = timestamp >> 15;
  tm_array[log_counter][1] = timestamp & 0x00007FFF;
  tm_array[log_counter][2] = mode;

  log_counter++;
  log_counter%=LOG_SIZE; //looping on the same arrays, so always logging

  //when the always log is not true, the logging will be performed only once
  if(log_counter == 0 && !always_log){
    send_err_message(LOG_FULL);
    log_data_completed = true;
  }
}

void log_sbias(int16_t sbias_array[],int32_t s_bias[])
{
  sbias_array[0] = s_bias[0];
  sbias_array[1] = s_bias[1];
  sbias_array[2] = s_bias[2];
  sbias_array[3] = s_bias[3];
  sbias_array[4] = s_bias[4];
  sbias_array[5] = s_bias[5];
}


//5sensors bias - RPYstablize -RP Also once: the bias, which are available after calibration
void log_control(Mode mode, int16_t control_array[][5],int32_t R_stablize,
  int32_t P_stabilize, int32_t Y_stabilize,int32_t R_angle, int32_t P_angle) {

      if(mode >= YAW_CONTROL)
      {
       //downcasting control values, right shift to keep the significant bits
       control_array[log_counter][0] = R_stablize;
       control_array[log_counter][1] = P_stabilize;
       control_array[log_counter][2] = Y_stabilize;
       control_array[log_counter][3] = RSHIFT(R_angle,4);
       control_array[log_counter][4] = RSHIFT(P_angle,4);
      }

}



//sensors and rpm
void log_sensors(int16_t sensor_array[][10], int32_t s0, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5,
  int16_t rpm0, int16_t rpm1, int16_t rpm2, int16_t rpm3) {

    	sensor_array[log_counter][0] = s0;
    	sensor_array[log_counter][1] = s1;
    	sensor_array[log_counter][2] = s2;
    	sensor_array[log_counter][3] = s3;
    	sensor_array[log_counter][4] = s4;
    	sensor_array[log_counter][5] = s5;

      sensor_array[log_counter][6] = rpm0;
      sensor_array[log_counter][7] = rpm1;
      sensor_array[log_counter][8] = rpm2;
      sensor_array[log_counter][9] = rpm3;

}


void log_event(int16_t event_array[][4],int32_t timestamp, char control, int16_t value)
{
         event_array[event_counter][0] = timestamp >> 15; //high
         event_array[event_counter][1] = timestamp & 0x00007FFF; //low
         event_array[event_counter][2] = control;
         event_array[event_counter][3] = value;

         event_counter++;
         event_counter%=EVENT_SIZE;

          if((event_counter == 0) && (!always_log)){
              send_err_message(LOG_FULL);
              log_ev_completed = true;
          }

}


/*Sends all the logs.
First line: CALIBRATION values
Each line after will be >>>>>  "time_h  time_l  mode s0-s5  rpm0-rpm3  R_s  P_s  Y_s  R_angle  P_angle" */
void send_logs(int16_t tm_array[][3], int16_t sbias_array[], int16_t sensor_array[][10], int16_t control_array[][5]) {
	int i;
	int j;
	PacketData p;
	//first line: CALIBRATION paramaters only
  for(i=0; i < 6; i++)
  {
    p.as_uint16_t = sbias_array[i];
    send_message(LOG_MSG_PART,p);
  }

  send_control_message(LOG_MSG_NEW_LINE);

  for(i=0; i < LOG_SIZE; i++)
  {
    for(j=0; j < 3; j++){
      p.as_uint16_t = tm_array[i][j];
      send_message(LOG_MSG_PART, p);
    }

		for(j = 0; j < 10; j++) {
			p.as_uint16_t = sensor_array[i][j];
			send_message(LOG_MSG_PART, p);
		}

    for(j = 0; j < 5; j++) {
      p.as_uint16_t = control_array[i][j];
      send_message(LOG_MSG_PART, p);
    //send_highlow(control_array[i][j]);
    }

		send_control_message(LOG_MSG_NEW_LINE);

		if(i%(LOG_SIZE/100)==(LOG_SIZE/100)-1){
			sprintf(message, "%i%%",i/100+1);
			send_term_message(message);
		}

	}//for i

	send_term_message("LOGGING COMPLETED.");
}

//send logs about events coming from keyboard and JS
//each line >>>>> "time_h time_l control value"
void send_logs_event(int16_t event_array[][4])
{
   int i,j;
   PacketData p;

   for(i=0; i < EVENT_SIZE; i++)
   {
      for(j=0; j < 4; j++){
         p.as_uint16_t = event_array[i][j];
         send_message(LOG_EVENT, p);
       }

       send_message(LOG_EV_NEW_LINE,p);
   }//for

}

#endif
