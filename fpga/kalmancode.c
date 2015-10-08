R_FILTER = 3;
    R_BIAS_UPDATE = 14;
   
    R_ANGLE = 5;
    R_ACC_RATIO = 2000;
    R_ACC_BIAS = round(mean(Racc(1:100)));
    
    C1_R = 7;
    C2_R = 16;

    P1_R = 6;
    P2_R = 5;

// kalman filter
// kalman init
dR = 0; // init (not very important)
Rangle = 0;
Rbias = bitshift_l(calibratedRgyro,R_BIAS_UPDATE);
filtered_dR = 0;
R_stabilize = 0;



/* kalman control loop */
//     substract bias:
    dR = bitshift_l(Rraw,R_BIAS_UPDATE)-Rbias;
//   filter
    filtered_dR+= - bitshift_l(filtered_dR,-R_FILTER) + bitshift_l(dR,-R_FILTER);
//     integrate for the angle and add something to react agianst
//     rounding error
    Rangle += bitshift_l(filtered_dR+bitshift_l(1,-R_BIAS_UPDATE+R_ANGLE-1),-R_BIAS_UPDATE+R_ANGLE);
    Rangle += - bitshift_l(Rangle-(Racc-R_ACC_BIAS)*R_ACC_RATIO+bitshift_l(1,C1_R-1),-C1_R);
//		update bias
    Rbias += bitshift_l(Rangle-(Racc-R_ACC_BIAS)*R_ACC_RATIO+ bitshift_l(1,C2_R-1),-C2_R);
//     calculate stabilization
    R_stabilize = bitshift_l(0-Rangle,-1*(R_BIAS_UPDATE - P1_R)) - bitshift_l(filtered_dR(k),-1*(R_BIAS_UPDATE - P2_R));

		