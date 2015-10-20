/* yawfilter */

/* Yaw filter parameters*/
int		Ybias = 400;
int 	filtered_dY = 0; //
#define	Y_BIAS_UPDATE 10 // update bias each sample with a fraction of 1/(2^Y_BIAS_UPDATE)
int	Y_filter = 3; // simple filter that updates 1/2^Y_FILTER
int 	P_yaw=10; // P = 2^4     Y_TO_ENGINE_SCALE
int 	dY;
int 	Y_stabilize;
#define YJS_TO_ANGLE_RATIO 2

#define increase_P_yaw() P_yaw++

#define decrease_P_yaw() P_yaw--

void setYbias(int newYbias){
	Ybias = newYbias;
}

/*YAW_CALCULATIONS*/
int yawControl(int yawRate, int Y_js){

	//  scale dY up with Y_BIAS_UPDATE
	dY 		= (yawRate << Y_BIAS_UPDATE) - Ybias;
	// filter dY
	filtered_dY 	+= - (filtered_dY >> Y_filter) + (dY >> Y_BIAS_UPDATE);
	// calculate stabilisation value
	if((Y_BIAS_UPDATE - P_yaw) >= 0) {
		Y_stabilize 	= (-Y_js*YJS_TO_ANGLE_RATIO- filtered_dY) >> (Y_BIAS_UPDATE - P_yaw); // calculate error of yaw rate
	} else {
		Y_stabilize 	=(-Y_js*YJS_TO_ANGLE_RATIO - filtered_dY) << (-Y_BIAS_UPDATE + P_yaw); // calculate error of yaw rate
	}
	return Y_stabilize;
}

void getYawParams(char *message){
	sprintf(message, "Y_stabilize = %i,  Ybias = %i, filtered_dY = %i\n", Y_stabilize, (Ybias >> Y_BIAS_UPDATE), (filtered_dY >> Y_BIAS_UPDATE));
}
