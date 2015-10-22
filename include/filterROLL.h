/*Roll parameters*/

#define	C2_R_BIAS_UPDATE 14 /* if you change this, change also C2_rounding_error and R_integrate_rounding_error*/
#define	R_ANGLE 5  /* if you change this, change also R_integrate_rounding_error*/
#define	R_ACC_RATIO 2500 //roll acceleration ratio
#define	C1_R 6
#define	C1_R_ROUNDING_ERROR  (1<<(C1_R-1)) //INCREASE_SHIFT(1,C1_R-1);
#define C2_R_ROUNDING_ERROR  (1<<(C2_R_BIAS_UPDATE-1)) /*INCREASE_SHIFT(1,C2_R_BIAS_UPDATE-1);*/
#define	R_INTEGRATE_ROUNDING_ERROR (1 << C2_R_BIAS_UPDATE-R_ANGLE-1) //INCREASE_SHIFT(1,C2_R_BIAS_UPDATE-R_ANGLE+1);

int32_t RJS_TO_ANGLE_RATIO = 300;

int32_t 	R_filter = 1;
int32_t		R_ACC_BIAS = 0;  /*set this in CALIBRATION mode*/
int32_t		P1_roll = 7; // watch out! if P1_roll is higher then C2_R_BIAS_UPDATE then things will go wrong 5
int32_t		P2_roll = 5; // watch out! if P2_roll is higher then C2_R_BIAS_UPDATE then things will go wrong 4
int32_t 	P1_roll_minor = 0;
int32_t 	P2_roll_minor = 0;

/*All the init should be done in a proper function*/
int32_t		dR = 0; // init (not very important what exact value)
int32_t		R_angle = 0; // init to zero
int32_t		Rbias = 0;// bitshift_l(calibratedRgyro,C2_R_BIAS_UPDATE);
int32_t		filtered_dR = 0;
int32_t 	R_stab1 =0, R_stab2=0;
int32_t		R_stabilize = 0;

/*ROLL_CALCULATIONS*/
int32_t rollControl(int32_t rollRate, int32_t roll_angle, int32_t R_js){
		//     substract bias and scale R:
		#ifdef DOUBLESENSORS
	    dR = INCREASE_SHIFT(rollRate,C2_R_BIAS_UPDATE-1)-Rbias;
		#else
			dR = INCREASE_SHIFT(rollRate,C2_R_BIAS_UPDATE)-Rbias;
		#endif /*DOUBLESENSORS*/

		// filter
	  filtered_dR+= - DECREASE_SHIFT(filtered_dR,R_filter) + DECREASE_SHIFT(dR,R_filter);

		// integrate for the angle and add something to react agianst
		// rounding error
	  R_angle += DECREASE_SHIFT(filtered_dR+R_INTEGRATE_ROUNDING_ERROR,  C2_R_BIAS_UPDATE-R_ANGLE);

		// kalman
		R_angle -= DECREASE_SHIFT(R_angle-(roll_angle*R_ACC_RATIO-R_ACC_BIAS)+C1_R_ROUNDING_ERROR,  C1_R);

		// update bias
	  Rbias += DECREASE_SHIFT(R_angle-(roll_angle*R_ACC_RATIO-R_ACC_BIAS)+C2_R_ROUNDING_ERROR,  C2_R_BIAS_UPDATE);

		// calculate stabilization
	  R_stab1 = DECREASE_SHIFT(R_js*RJS_TO_ANGLE_RATIO+R_angle,  C2_R_BIAS_UPDATE - P1_roll);
		R_stab2 = DECREASE_SHIFT(filtered_dR,  C2_R_BIAS_UPDATE - P2_roll);
		R_stabilize = R_stab1+R_stab2 + (R_stab1>>2)*P1_roll_minor+(R_stab2>>2)*P2_roll_minor;

		return R_stabilize;
}
