//THIS FILE CONTAINS ALL THE OLD FUNCTIONS THAT ARE NOW NOT NECESSARY BUT THAT CAN BE STILL USEFUL (MAYBE)
/* Prints the value to the log file with #awesomesauce
   Author: Alessio
*/
void print_value_to_file(unsigned char c)
{
	int a,b;
	unsigned char uns = (unsigned char) c;

   //even: read most significant bytes
	 if(value_counter % 2 == 0) {
		 value_to_print = uns;
		 value_to_print = (value_to_print << 8) & 0xFF00;

		 //a = (int) c;
		 //fprintf(log_file,"%i ",a);
	 }

  //odd: read least signifative bytes and print the value
	 else {
		 value_to_print |= uns;

		fprintf(log_file,"%hu ",value_to_print);
		value_to_print = 0;

		//b = (int) c;
    //	fprintf(log_file,"%i ",b);
		// if(value_counter % 14 == 1) fprintf(log_file, ": ");
	 }

	 value_counter++;
}


/*Send real-time feedback from the QR.
Included: Timestamp mode sensors[6] RPM, control and signal proc chain values, telemetry.
Author: Alessio*/
void send_feedback()
{
	char fb_msg[250];
	//Real Time Data: Timestamp mode sensors[6] ae[4] R&P&Ystabilization R&Pangle Joystick changes
  sprintf(message,"X32_ms_clock:%i M=%i Sensor bias = [%i,%i,%i,%i,%i,%i]\n",X32_ms_clock,mode,s_bias[0] >> SENSOR_PRECISION,s_bias[1] >> SENSOR_PRECISION,s_bias[2] >> SENSOR_PRECISION,s_bias[3] >> SENSOR_PRECISION,s_bias[4] >> SENSOR_PRECISION,s_bias[5] >> SENSOR_PRECISION);
	strcat(fb_msg,message);

	sprintf(message,"offset = [%i,%i,%i,%i] rpm = [%i,%i,%i,%i]\n",get_motor_offset(0),get_motor_offset(1),get_motor_offset(2),get_motor_offset(3),
	get_motor_rpm(0),get_motor_rpm(1),get_motor_rpm(2),get_motor_rpm(3));
  strcat(fb_msg,message);

	if(mode < YAW_CONTROL)
	 send_feedback_message(fb_msg);
	else
	{
		sprintf(message,"Control loop time(us) = %i P:[r1=%i r2=%i,p=%i,y=%i] # R_stab=%i P_stab=%i Y_stab=%i # R_angle=%i P_angle=%i\n",control_loop_time,P1_roll,P2_roll,P_pitch, P_yaw, R_stabilize,P_stabilize,Y_stabilize,R_angle,P_angle);
		strcat(fb_msg,message);
		send_feedback_message(fb_msg);
	}
}
