/*Pitch parameters*/
#define	P_FILTER 1
#define	C2_P_BIAS_UPDATE 14 /* if you change this, change also C2_rounding_error and P_integrate_rounding_error*/
#define	P_ANGLE 5  /* if you change this, change also P_integrate_rounding_error*/
#define	P_ACC_RATIO 2500
#define	C1_P 6
#define	C1_P_ROUNDING_ERROR  (1<<(C1_P-1)) //INCPEASE_SHIFT(1,C1_P-1);
#define C2_P_ROUNDING_ERROR  (1<<(C2_P_BIAS_UPDATE-1)) /*INCPEASE_SHIFT(1,C2_P_BIAS_UPDATE-1);*/
#define	P_INTEGRATE_ROUNDING_ERROR (1<<C2_P_BIAS_UPDATE-P_ANGLE-1) //INCPEASE_SHIFT(1,C2_P_BIAS_UPDATE-P_ANGLE+1);
#define PJS_TO_ANGLE_RATIO 300

int		P_ACC_BIAS = 0;  /*set this in CALIBPATION mode*/
int		P1_pitch = 7; // watch out! if P1_roll is higher then C2_P_BIAS_UPDATE then things will go wrong 5
int		P2_pitch = 6; // watch out! if P2_roll is higher then C2_P_BIAS_UPDATE then things will go wrong 5
int P1_pitch_minor = 0;
int P2_pitch_minor = 0;

/*All the init should be done in a proper function*/
int		dP = 0; // init (not very important what exact value)
int		P_angle = 0; // init to zero
int		Pbias = 0;// bitshift_l(calibratedPgyro,C2_P_BIAS_UPDATE);
int		filtered_dP = 0;
int 	P_stab1 =0, P_stab2=0;
int		P_stabilize = 0;

/*PITCH_CALCULATIONS*/
int pitchControl(int pitchRate,int pitch_angle, int P_js){
		//     substract bias and scale P:
	    dP = INCREASE_SHIFT(pitchRate,C2_P_BIAS_UPDATE)-Pbias;
		//   filter
	    filtered_dP+= - DECREASE_SHIFT(filtered_dP,P_FILTER) + DECREASE_SHIFT(dP,P_FILTER);
		//     integrate for the angle and add something to react agianst
		//     rounding error
	    P_angle += DECREASE_SHIFT(filtered_dP+P_INTEGRATE_ROUNDING_ERROR, C2_P_BIAS_UPDATE-P_ANGLE);
	    // kalman
		P_angle -= DECREASE_SHIFT(P_angle-(pitch_angle*P_ACC_RATIO-P_ACC_BIAS)+C1_P_ROUNDING_ERROR, C1_P);
		//		update bias
	    Pbias += DECREASE_SHIFT(P_angle-(pitch_angle*P_ACC_RATIO-P_ACC_BIAS)+C2_P_ROUNDING_ERROR, C2_P_BIAS_UPDATE);
	//     calculate stabilization
	    P_stab1 = DECREASE_SHIFT(P_js*PJS_TO_ANGLE_RATIO-P_angle,C2_P_BIAS_UPDATE - P1_pitch);
		P_stab2	= DECREASE_SHIFT(filtered_dP,C2_P_BIAS_UPDATE - P2_pitch);
		P_stabilize = P_stab1+P_stab2 + (P_stab1>>2)*P1_pitch_minor+(R_stab2>>2)*P2_pitch_minor;
		return P_stabilize;
}
