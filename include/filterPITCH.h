/*Pitch parameters*/

#define	C2_P_BIAS_UPDATE 14 /* if you change this, change also C2_rounding_error and P_integrate_rounding_error*/
#define	P_ANGLE 5  /* if you change this, change also P_integrate_rounding_error*/
#define	P_ACC_RATIO 2500
#define	C1_P 6
#define	C1_P_ROUNDING_ERROR  (1 << (C1_P - 1)) //INCREASE_SHIFT(1,C1_P-1);
#define C2_P_ROUNDING_ERROR  (1 << (C2_P_BIAS_UPDATE - 1)) /*INCREASE_SHIFT(1,C2_P_BIAS_UPDATE-1);*/
#define	P_INTEGRATE_ROUNDING_ERROR (1 << C2_P_BIAS_UPDATE - P_ANGLE - 1) //INCREASE_SHIFT(1,C2_P_BIAS_UPDATE-P_ANGLE+1);

int32_t 	PJS_TO_ANGLE_RATIO = 300;

int32_t 	P_filter = 1;
int32_t		P_ACC_BIAS = 0;  /*set this in CALIBPATION mode*/
int32_t		P1_pitch = 7; // watch out! if P1_roll is higher then C2_P_BIAS_UPDATE then things will go wrong 5
int32_t		P2_pitch = 5; // watch out! if P2_roll is higher then C2_P_BIAS_UPDATE then things will go wrong 5
int32_t 	P1_pitch_minor = 0;
int32_t 	P2_pitch_minor = 0;

/*All the init should be done in a proper function*/
int32_t		dP = 0; // init (not very important what exact value)
int32_t		P_angle = 0; // init to zero
int32_t		Pbias = 0;// bitshift_l(calibratedPgyro,C2_P_BIAS_UPDATE);
int32_t		filtered_dP = 0;
int32_t 	P_stab1 = 0, P_stab2 = 0;
int32_t		P_stabilize = 0;

/*PITCH_CALCULATIONS*/
int32_t pitchControl(int32_t pitchRate, int32_t pitch_angle, int32_t P_js) {
		//     substract bias and scale P:
		#ifdef DOUBLESENSORS
	    	dP = INCREASE_SHIFT(pitchRate,C2_P_BIAS_UPDATE-1)-Pbias;
		#else
			dP = INCREASE_SHIFT(pitchRate,C2_P_BIAS_UPDATE)-Pbias;
		#endif /*DOUBLESENSORS*/

		//   filter
	  filtered_dP+= - DECREASE_SHIFT(filtered_dP,P_filter) + DECREASE_SHIFT(dP,P_filter);

		// integrate for the angle and add something to react agianst
		// rounding error
	  P_angle += DECREASE_SHIFT(filtered_dP+P_INTEGRATE_ROUNDING_ERROR, C2_P_BIAS_UPDATE-P_ANGLE);

		// kalman
		P_angle -= DECREASE_SHIFT(P_angle-(pitch_angle*P_ACC_RATIO-P_ACC_BIAS)+C1_P_ROUNDING_ERROR, C1_P);

		// update bias
	  Pbias += DECREASE_SHIFT(P_angle-(pitch_angle*P_ACC_RATIO-P_ACC_BIAS)+C2_P_ROUNDING_ERROR, C2_P_BIAS_UPDATE);

		// calculate stabilization
	  P_stab1 = DECREASE_SHIFT(P_js*PJS_TO_ANGLE_RATIO-P_angle,C2_P_BIAS_UPDATE - P1_pitch);
		P_stab2	= DECREASE_SHIFT(filtered_dP,C2_P_BIAS_UPDATE - P2_pitch);
		P_stabilize = P_stab1+P_stab2 + (P_stab1>>2)*P1_pitch_minor+(R_stab2>>2)*P2_pitch_minor;

		return P_stabilize;
}
