#include "keysandjs.h"
#include <stdarg.h>

int fd_RS232, fd_js;

/* current axis and button readings
 */
int	axis[6];
bool axisflags[6];
int	button[12];
FILE *log_file;
FILE *terminal_log_file;
int timers[3];
char received_chars[QR_INPUT_BUFFERSIZE];
char terminal_message[QR_INPUT_BUFFERSIZE];
int charpos = 0;
char fb_msg[QR_INPUT_BUFFERSIZE];
int fb_ch =0; //feedback message char position
int fb_msg_counter = 0;
Fifo qr_msg_q;
int in_packet_counter=0;
int out_packet_counter=0;

int sensors[6];
int QR_r, QR_p = 0;
int QR_rs, QR_ps, QR_ys = 0;
int loopcount = 0; // to calculate the FPS
int RPM[4];
Mode QRMode = SAFE;
char terminal_message[QR_INPUT_BUFFERSIZE];
int ms_last_packet_sent;
char last_out_message[4];
struct timeval keep_alive;
char error_message[100];


//***********
/* Main function that mainly consists of polling the different connections
 * which are: Keyboard, Joystick, RS232 (connection to QR)
 */
int main (int argc, char **argv)
{
	struct js_event js;
	int i;
	int c, last_c;
	struct timeval time, last_packet_time;
	int rec_c;

	setenv("ESCDELAY", "25", 0); // necessary to a fast detection of pressing esc (char == 27)
	initscr();

	start_color();
	init_pair(0, COLOR_WHITE, COLOR_BLACK);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);

  	init_keyboard();
	init_joystick();
	rs232_open(fd_RS232);
	init_log();

	gettimeofday(&time,NULL);
	gettimeofday(&keep_alive,NULL);
	gettimeofday(&last_packet_time,NULL);

	drawBase();

	while(1) {
		// Transform all data
		check_alive_connection();

		/* Check keypress */
		if ((c= getch()) != -1){
			sendKeyData(c); // send a message if user gave input
		}

		/* Check joystick */
		if(fd_js>0){
			while (read(fd_js, &js, sizeof(struct js_event)) ==
				   			sizeof(struct js_event))  {
				save_JS_event(js.type,js.number,js.value);
			}
			last_packet_time = sendJSData(last_packet_time);
		}

		/* Check QR to pc communication */
		if(fd_RS232>0){
			while ((rec_c = rs232_getchar_nb(fd_RS232))!= -1000){
				fifo_put(&qr_msg_q, rec_c);
				check_msg_q();
			}
		}

		// Options to quit the program
		if (button[0]){
			pc_send_message(SPECIAL_REQUEST, ESCAPE);
		}
		if(c == 27){
			pc_send_message(SPECIAL_REQUEST, ESCAPE);
			break;
		}

		// Draw everything
		clearMessages();
		
		drawMode(QRMode);		
		drawJS(axis[0],axis[1],axis[2],axis[3]);
		drawSensors(sensors);
		drawAngles(QR_r,QR_p);
		drawControl(QR_rs,QR_ps,QR_ys);
		drawCommunication(in_packet_counter, out_packet_counter);
		displayMessage(last_out_message, error_message, terminal_message);		
		drawRPM(RPM[0],RPM[1],RPM[2],RPM[3]);		

		refresh();
	}

	exitmain();
	return 0;
}

void pt(int y, int x, char* format, ...) {
	va_list args;
	va_start( args, format );
	move(y,x);
	vwprintw(stdscr, format, args);
	va_end( args );
}

void ptb(int y, int x, char* format, ...) {
	attron(A_BOLD);		
	va_list args;
	va_start( args, format );
	move(y,x);
	vwprintw(stdscr, format, args);
	va_end( args );
	attroff(A_BOLD);		
}

void ptc(int y, int x, int c, char* format, ...) {
	col_on(c);	
	va_list args;
	va_start( args, format );
	move(y,x);
	vwprintw(stdscr, format, args);
	va_end( args );
	col_off(c);		
}

void ptbc(int y, int x, int c, char* format, ...) {
	col_on(c);
	attron(A_BOLD);		
	va_list args;
	va_start( args, format );
	move(y,x);
	vwprintw(stdscr, format, args);
	va_end( args );
	attroff(A_BOLD);
	col_off(c);		
}

// Draw all info on the screen
void drawBase() {
	int margin = 2;
	int tab_size = 20;
	int y_spacing = 2;

	// Mode
	ptb(1,2,"Mode");

	// Joystick
	ptbc(3,2,2,"Joystick");		
	ptbc(3,16,2,"R = ");
	ptbc(4,16,2,"P = ");
	ptbc(5,16,2,"Y = ");
	ptbc(6,16,2,"L = ");

	// Sensors
	ptbc(8,2,3,"Sensors");
	ptbc(8,15,3,"s0 = ");
	ptbc(9,15,3,"s1 = ");
	ptbc(10,15,3,"s2 = ");
	ptbc(11,15,3,"s3 = ");
	ptbc(12,15,3,"s4 = ");
	ptbc(13,15,3,"s5 = ");

	// Angles
	ptbc(15,2,4,"Angles");
	ptbc(15,16,4,"R = ");
	ptbc(16,16,4,"P = ");

	// Control values
	ptbc(18,2,5,"Control");
	ptbc(18,14,5,"R_s = ");
	ptbc(19,14,5,"P_s = ");
	ptbc(20,14,5,"Y_s = ");

	// Packet info
	ptbc(22,2,6,"Comm.");
	ptbc(22,13,6,"#out = ");
	ptbc(23,13,6,"#in  = ");

	// Messages
	ptb(25,2,"Messages");
	ptb(25,14,"out = ");
	ptbc(26,14,1,"err = ");
	ptb(27,14,"in  = ");
	
	// QR
	ptb(3,55,"|");
	ptb(4,55,"|");
	ptb(5,50,"---- + ----");
	ptb(6,55,"|");
	ptb(7,55,"|");

	refresh();
}

void drawMode(Mode m) {
	ptb(1,20,"%-20s",mode_to_string(m));
}

void drawJS(int R, int P, int Y, int T) {
	ptbc(3,20,2,"%-3d",R);
	ptbc(4,20,2,"%-3d",P);
	ptbc(5,20,2,"%-3d",Y);
	ptbc(6,20,2,"%-3d",T);
}

void drawSensors(int sensors[6]) {
	ptbc(8,20,3,"%-16d",sensors[0]);
	ptbc(9,20,3,"%-16d",sensors[1]);
	ptbc(10,20,3,"%-16d",sensors[2]);
	ptbc(11,20,3,"%-16d",sensors[3]);
	ptbc(12,20,3,"%-16d",sensors[4]);
	ptbc(13,20,3,"%-16d",sensors[5]);
}

void drawAngles(int R, int P) {
	ptbc(15,20,4,"%-16d",R);
	ptbc(16,20,4,"%-16d",P);
}

void drawControl(int R_s, int P_s, int Y_s) {
	ptbc(18,20,5,"%-16d",R_s);
	ptbc(19,20,5,"%-16d",P_s);
	ptbc(20,20,5,"%-16d",Y_s);
}

void drawCommunication(int in_packets, int out_packets) {
	ptbc(22,20,6,"%-32d",out_packets);
	ptbc(23,20,6,"%-32d",in_packets);
}

void displayMessage(char* message, char* error, char* terminal_message) {
	move(25,20); clrtoeol();
	ptb(25,20,"%s",message);
	move(26,20); clrtoeol();
	attron(A_UNDERLINE);
	ptbc(26,20,1,"%s",error);
	attroff(A_UNDERLINE);
	move(27,20); clrtoeol();
	ptb(27,20,"%s",terminal_message);
}

void drawRPM(int m0, int m1, int m2, int m3) {
	ptb(5,45,"%4d", m3);
	ptb(5,62,"%-4d", m1);
	ptb(2,55,"%-4d", m0);
	ptb(8,55,"%-4d", m2);
}	

/* Calculate the mean loop frequency (100 loopcount mean)
 * Author: Henko Aantjes
 */
struct timeval updateFPS(struct timeval timeold){
	struct timeval timenew;
	if(loopcount++>99){
		loopcount=0;
		gettimeofday(&timenew,NULL);
		int frametime = (timenew.tv_usec+1000000*timenew.tv_sec-timeold.tv_usec-1000000*timeold.tv_sec)/100;
		col_on(9);
		////ptb(LINE_NR_FPS,0,"    pc looptime: %3i usec (%6i Hz)                          \n",frametime,1000000/frametime);
		col_on(9);
		return timenew;
	} else {
		return timeold;
	}
}

/* Check the time that the terminal message is shown and
 * delete the message if that time is too long
 * Author: Henko Aantjes
 */
void clearMessages(void){
	int i,j;
	if(timers[0] != -1){
		if(timers[0]++> MAX_MSG_TIME){
			sprintf(terminal_message," ");
			timers[0] = -1;
		}
	}
	if(timers[1] != -1){
		if(timers[1]++>MAX_ERROR_MSG_TIME){
			sprintf(error_message," ");
			timers[1] = -1;
		}
	}

	/*if(timers[2] != -1){
		if(timers[2]++>MAX_ERROR_MSG_TIME){
			j = (timers[2]-MAX_ERROR_MSG_TIME)/1000;
			for(i=0; i < 7; i++) {
				move(LINE_NR_QR_STATE+i,0);
				clrtoeol();
			}
			timers[2] = -1;
		}
	}*/
}

/* Process a joystick event
 * Author: Henko Aantjes
 */
void save_JS_event(int type, int number,int value){
	switch(type & ~JS_EVENT_INIT) {
		case JS_EVENT_BUTTON:
			button[number] = value;
			break;
		case JS_EVENT_AXIS:
			if(number == 3){
				axis[number] = value/-256 + 127;
			} else {
				axis[number] = value/256;
			}
			axisflags[number] = true;
			break;
	}
}

/* Initialize the key input
 *
 * Henko Aantjes
 */
void init_keyboard(void){
	keypad(stdscr, TRUE); // enable arrowkey-detection - This also makes ncurses full-screen for some reason
	noecho(); // don't print what is typed
	cbreak(); // don't wait for an enter
	nodelay(stdscr, TRUE); // don't wait for user input, give error instead
}

/* Change color of terminal output */
void col_on(int col){
	attron(COLOR_PAIR(col));
}
/* reset color of terminal output */
void col_off(int col){
	attroff(COLOR_PAIR(col));
}

/* Initialize joystick connection
 * Author: Henko Aantjes
 */
int init_joystick(void){
	int i;
	if ((fd_js = open(JS_DEV, O_RDONLY)) < 0) { // open connection
		printw("Joystick not found. <<press a key to continue>>");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
		clear();
		refresh();
	} else {
		fcntl(fd_js, F_SETFL, O_NONBLOCK); // set joystick reading non-blocking
	}

	for (i = 0; i < 6; i++) {
		axis[i] = -1;
	}
	for (i = 0; i < 6; i++) {
		axisflags[i] = false;
	}

	for (i = 0; i < 12; i++) {
		button[i]= 0;
	}

	return 0;
}

/* initialize the log file
 * Author: Henko Aantjes
 */
void init_log(void){

	log_file = fopen("flight_log2.txt", "w");

	if (log_file == NULL)
	{
		printw("Error opening log file. <<press a key to continue>>");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
		clear();
		refresh();
	}
}

/* Checks if the PC has sent data in the last 200ms otherwise sends
a packet to keep alive the connection.
Author: Alessio */
void check_alive_connection()
{
	gettimeofday(&keep_alive,NULL);
	int current_ms = ((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000);
	if(current_ms - ms_last_packet_sent > TIMEOUT)
	{
		pc_send_message(0,0);
	}
	return;
}


/* Send the user keyboard input
 * First construct the message and call the send function
 * Author: Henko Aantjes
 */
void sendKeyData(int c){
	if(fd_RS232 <= 0) return;
	char control, value = 0; // the control and value to send

	if(c >= '0' && c <= '5') { // if c is a mode change
		value = (char) c-'0';
		control = 'M';

		// Extra check that not sends mode changes if the joystick values are not all zero
		if((axis[0]==0 && axis[1]==0 && axis[2]==0 && axis[3]==0) || fd_js<0)
			pc_send_message(control, value);
		else
			print_error_message(JS_LIFT_NOT_ZERO);
	} else {
		control = 'A'; // A == Adjust trimming

		switch(c){
			case KEY_LEFT:
				value = ROLL_LEFT;
				break;
			case KEY_RIGHT:
				value = ROLL_RIGHT;
				break;
			case KEY_UP:
				value = PITCH_DOWN;
				break;
			case KEY_DOWN:
				value = PITCH_UP;
				break;
			case LIFT_UP:
				value = LIFT_UP;
				break;
			case LIFT_DOWN:
				value = LIFT_DOWN;
				break;
			case YAW_LEFT:
				value = YAW_LEFT;
				break;
			case YAW_RIGHT:
				value = YAW_RIGHT;
				break;
			case P_YAW_UP:
				value = P_YAW_UP;
				break;
			case P_YAW_DOWN:
				value = P_YAW_DOWN;
				break;
			case P_ROLL_UP:
				value = P_ROLL_UP;
				break;
			case P_ROLL_DOWN:
				value = P_ROLL_DOWN;
				break;
			case P_PITCH_UP:
				value = P_PITCH_UP;
				break;
			case P_PITCH_DOWN:
				value = P_PITCH_DOWN;
				break;
			case ASK_MOTOR_RPM:
				control = SPECIAL_REQUEST;
				value = ASK_MOTOR_RPM;
				break;
			case ASK_FILTER_PARAM:
				control = SPECIAL_REQUEST;
				value = ASK_FILTER_PARAM;
				break;
			case ASK_FULL_CONTROL_PARAM:
				control = SPECIAL_REQUEST;
				value = ASK_FULL_CONTROL_PARAM;
				break;
			case RESET_MOTORS:
				control = SPECIAL_REQUEST;
				value = RESET_MOTORS;
				break;
			case RESET_SENSOR_LOG:
				control = SPECIAL_REQUEST;
				value = RESET_SENSOR_LOG;
				break;
			case ASK_SENSOR_LOG:
			  	control = SPECIAL_REQUEST;
				value = ASK_SENSOR_LOG;
				break;
			case ASK_SENSOR_BIAS:
				control = SPECIAL_REQUEST;
				value = ASK_SENSOR_BIAS;
				break;
			case ESCAPE: // ESCAPEKEY
				control = SPECIAL_REQUEST;
				value = ESCAPE;
				break;
			default:
				value = 0;
				break;
		}

		if(value != 0)
			pc_send_message(control, value);
	}
}

/* Send Joystick data
 * Construct a message and send
 * Author: Henko Aantjes
 */
struct timeval sendJSData(struct timeval last_packet_time){
	if(fd_RS232 <= 0) return;
	struct timeval timenew;
	int number, control, value;

	for(number=0;number<4;number++){
		if(axisflags[number]){
			switch(number){
				case 0:
					control = JS_ROLL; // roll
					break;
				case 1:
					control = JS_PITCH; // pitch
					break;
				case 2:
					control = JS_YAW; // yaw
					break;
				case 3:
					control = JS_LIFT; // throttle
					break;
				default:
					control = 0;
					break;
			}

			if(control == 0) return;

			gettimeofday(&timenew,NULL);
			int timediff = timenew.tv_usec+1000000*timenew.tv_sec-last_packet_time.tv_usec-1000000*last_packet_time.tv_sec;
			
			if(timediff < 1000 * 10){
				return last_packet_time;
			} else {
				axisflags[number] = false;
				pc_send_message(control, axis[number]);
				return timenew;
			}
		}
	}
}

/*Swap the endianess for 32 bits unsigned int.
Author: Alessio*/
uint32_t swap_endianess_32(uint32_t num){
	return ((num>>24)&0xFF) | // move byte 3 to byte 0
          ((num>>8)&0xFF00) | // move byte 2 to byte 1
						((num<<8)&0xFF0000) | // move byte 1 to byte 2
              ((num<<24)&0xFF000000); // byte 0 to byte 3
}

/*Swap the endianess for 16bits unsigned int.
Author: Alessio*/
uint16_t swap_endianess_16(uint16_t val)
{
	return (val >> 8) | (val << 8 );
}

/*Print log values to file, taking in account the endianess
Author: Alessio */
void print_log_to_file(PacketData data)
{
	//uint16_t val = data.as_uint16_t;
	//val = swap_endianess_16(val);
	fprintf(log_file, "%hu ", data.as_uint16_t);
}


/* Parse the QR input (one char at the time)
 * Call parse message if a message is complete
 * Originally created by: Henko Aantjes
 */
void packet_received(char control, PacketData data){
	in_packet_counter++;

	// Change endianness
	PacketData swapped;
	swapped = swap_byte_order(data);
	char *ret;

	int i;
	char valueChar = swapped.as_bytes[1];
	uint16_t value = swapped.as_uint16_t;

	switch(control){
		case TERMINAL_MSG_START: // start new qr terminal message (not necessary)
			charpos = 0;
			break;
		case TERMINAL_MSG_PART: // characters of the terminal message
			if(charpos<QR_INPUT_BUFFERSIZE)
				received_chars[charpos++]= valueChar;
			break;
		case TERMINAL_MSG_FINISH:
			sprintf(terminal_message, "%.*s", charpos, received_chars);

			timers[0] = 0;
			charpos = 0;
			break;
		/*Cases to print QR state in real-time and logging data
		Author: Alessio*/
   		case CURRENT_MODE:
			QRMode = value;
			break;
		case TIMESTAMP:
		    //ptb(LINE_NR_QR_STATE,20,"TIMESTAMP: %4i",value);
			break;
		case SENS_0:
			sensors[0] = value;
			break;
		case SENS_1:
			sensors[1] = value;
			break;
		case SENS_2:
			sensors[2] = value;
			break;
		case SENS_3:
			sensors[3] = value;
			break;
		case SENS_4:
			sensors[4] = value;
			break;
		case SENS_5:
			sensors[5] = value;
			break;
		case RPM0:
			RPM[0] = value;
			break;
		case RPM1:
			RPM[1] = value;
		break;
    	case RPM2:
			RPM[2] = value;
		break;
		case RPM3:
			RPM[3] = value;
		break;
		case MY_STAB:
			QR_ys = value;
			break;
		case MP_STAB:
			QR_ps = value;
     			break;
		case MR_STAB:
			QR_rs = value;
    			 break;
		case MP_ANGLE:
			QR_p = value;
     			break;
		case MR_ANGLE:
			QR_r = value;
     			break;
		case FB_MSG:
		  if(fb_ch < QR_INPUT_BUFFERSIZE)
			  fb_msg[fb_ch++] = valueChar;
				break;
		case FB_MSG_END:
		  col_on(4);
			//ptb(LINE_NR_QR_STATE-1,0,"<<QR Real-time data>>");
			col_off(4);
			col_on(7);
	    //ptb(LINE_NR_QR_STATE,0,"%s",fb_msg);
			col_off(7);
		  timers[2] = 0;
	    fb_ch = 0;
			break;
		case LOG_MSG_PART:
	    		print_log_to_file(swapped);
			break;
		case LOG_MSG_NEW_LINE:
			fprintf(log_file,"\n");
			break;
		case ERROR_MSG:
		  	print_error_message(value);
			break;
		default:
			break;
	}
}

/*Displays the error message.
Author: Alessio */
print_error_message(Error err)
{
	switch(err) {
		case LOG_ONLY_IN_SAFE_MODE:
			sprintf(error_message,"[QR]: Switch to SAFE before asking for the logging.");
			break;
		case MODE_ILLIGAL:
			sprintf(error_message,"[QR]: Invalid or illigal mode.]");
			break;
	 	case MODE_CHANGE_ONLY_VIA_SAFE:
			sprintf(error_message, "[QR]: Mode can be changed only from SAFE mode.");
			break;
		case MODE_CHANGE_ONLY_IF_ZERO_RPM:
			sprintf(error_message, "[QR]: Cannot change mode. RPM are not zero.");
			//pc_send_message(SPECIAL_REQUEST,ASK_MOTOR_RPM); //when RPM will be visualized in real-time, this won't be needed
			break;
		case MODE_ALREADY_SET:
			sprintf(error_message, "[QR]: Mode already changed.");
			break;
		case CONTROL_DISABLED_IN_THIS_MODE:
			sprintf(error_message, "[QR]: Manual control disabled in this mode. ");
			break;
		case JS_LIFT_NOT_ZERO:
			sprintf(error_message, "[PC]: Make sure JS- lift is zero. ");
			break;
		case SENSOR_LOG_FULL:
			sprintf(error_message, "[QR]: Sensor log is full. ");
			break;
		case FIRST_CALIBRATE:
			sprintf(error_message, "[QR]: You first need to calibrate! ");
			break;
		default:
			sprintf(error_message, "[PC] Wrong! not recognized. Wrong error code.");
	}

	timers[1] = 0;
}


void check_msg_q(){
	while(fifo_size(&qr_msg_q) >= 4) { // Check if there are one or more packets in the queue
		char control;
		PacketData data;
		char checksum;

		fifo_peek_at(&qr_msg_q, &control, 0);
		fifo_peek_at(&qr_msg_q, &data.as_bytes[0], 1);
		fifo_peek_at(&qr_msg_q, &data.as_bytes[1], 2);
		fifo_peek_at(&qr_msg_q, &checksum, 3);

		if(!check_packet(control,data,checksum)) {
			// If the checksum is not correct, pop the first message off the queue and repeat the loop
			char c;
			fifo_pop(&qr_msg_q, &c);
		} else {
			// If the checksum is correct, pop the packet off the queue and notify a callback
			char c;
			fifo_pop(&qr_msg_q, &c);
			fifo_pop(&qr_msg_q, &c);
			fifo_pop(&qr_msg_q, &c);
			fifo_pop(&qr_msg_q, &c);

			if(control != 0){
				packet_received(control, data);
			}
		}
	}
}



/* Exit routine
 * Author: Henko Aantjes
 */
void exitmain(void){
	sendKeyData(ESCAPE);
	if(fd_RS232>0){
  		close(fd_RS232);
	}
	endwin();
	fclose(log_file);
}
