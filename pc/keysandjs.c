#include "keysandjs.h"
#include "layout.h"
#include <stdarg.h>

int fd_RS232, fd_js;

/* current axis and button readings
 */

// Joystick
int	axis[6];
bool 	axisflags[6];
int	button[12];

FILE 	*log_file;
FILE 	*log_file_event;
int 	timers[2];

char 	error_message[100];
char 	received_chars[QR_INPUT_BUFFERSIZE];
char 	terminal_message[QR_INPUT_BUFFERSIZE];
int 	charpos = 0;

Fifo	qr_msg_q;
int 	in_packet_counter=0;
int 	out_packet_counter=0;
int 	ms_last_packet_sent;
char 	last_out_message[4];
struct 	timeval keep_alive;


int 	sensors[6];
int 	QR_r, QR_p = 0;
int 	QR_rs, QR_ps, QR_ys = 0;
int 	RPM[4];
Mode 	QRMode = SAFE;
int 	loopcount = 0; // to calculate the FPS

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
	MEVENT event;

	setenv("ESCDELAY", "25", 0); // necessary to a fast detection of pressing esc (char == 27)
	initscr();
	mousemask(BUTTON1_CLICKED, NULL);
	init_colors();
  	init_keyboard();
	init_joystick();
	rs232_open(fd_RS232);
	init_log();

	gettimeofday(&time,NULL);
	gettimeofday(&keep_alive,NULL);
	gettimeofday(&last_packet_time,NULL);

	drawBase();
	refresh();

	while(1) {
		// Transform all data
		check_alive_connection();

		/* Check keypress */
		if ((c= getch()) != -1){
			sendKeyData(c); // send a message if user gave input
			if(c == KEY_MOUSE)
				if(getmouse(&event) == OK)
					processMouse(event.bstate,event.y,event.x);
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
			if(fd_RS232 > 0)
				pc_send_message(SPECIAL_REQUEST, ESCAPE);
		}
		if(c == 27){
			if(fd_RS232 > 0) {
				pc_send_message(SPECIAL_REQUEST, ESCAPE);
			}
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

	log_file = fopen("flight_log.txt", "w");
	log_file_event = fopen("flight_log_event.txt", "w");

	if (log_file == NULL || log_file_event == NULL)
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

void processMouse(int button, int line, int x){
	if(button ==BUTTON1_CLICKED){
		//mvprintw(line,x, ".");
		switch(line){
			case(18):
				if(x==55)
					pc_send_message('A',P_YAW_DOWN);
				if(x==62)
					pc_send_message('A',P_YAW_UP);
			break;
			case(19):
				if(x==55)
					pc_send_message('A',P1_DOWN);
				if(x==62)
					pc_send_message('A',P1_UP);
			break;
			case(20):
				if(x==55)
					pc_send_message('A',P2_DOWN);
				if(x==62)
					pc_send_message('A',P2_UP);
			break;
		}
	}
}

/* Send the user keyboard input
 * First construct the message and call the send function
 * Author: Henko Aantjes
 */
void sendKeyData(int c){
	char control, value = 0; // the control and value to send

	if(c >= '0' && c <= '5') { // if c is a mode change
		value = (char) c-'0';
		control = 'M';

		// Extra check that not sends mode changes if the joystick values are not all zero
		if((axis[0]==0 && axis[1]==0 && axis[2]==0 && axis[3]==0) || fd_js<0 || value == SAFE || value == PANIC)
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
			case P1_UP:
				value = P1_UP;
				break;
			case P1_DOWN:
				value = P1_DOWN;
				break;
			case P2_UP:
				value = P2_UP;
				break;
			case P2_DOWN:
				value = P2_DOWN;
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

/*Print log values to file, taking in account the endianess
Author: Alessio */
void print_log_to_file(PacketData data)
{
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
			sensors[0] = swapped.as_int16_t;
			break;
		case SENS_1:
			sensors[1] = swapped.as_int16_t;
			break;
		case SENS_2:
			sensors[2] = swapped.as_int16_t;
			break;
		case SENS_3:
			sensors[3] = swapped.as_int16_t;
			break;
		case SENS_4:
			sensors[4] = swapped.as_int16_t;
			break;
		case SENS_5:
			sensors[5] = swapped.as_int16_t;
			break;
		case RPM0:
			RPM[0] = swapped.as_int16_t;
			break;
		case RPM1:
			RPM[1] = swapped.as_int16_t;
			break;
    		case RPM2:
			RPM[2] = swapped.as_int16_t;
			break;
		case RPM3:
			RPM[3] = swapped.as_int16_t;
			break;
		case MY_STAB:
			QR_ys = swapped.as_int16_t;
			break;
		case MP_STAB:
			QR_ps = swapped.as_int16_t;
     			break;
		case MR_STAB:
			QR_rs = swapped.as_int16_t;
    			 break;
		case MP_ANGLE:
			QR_p = swapped.as_int16_t;
     			break;
		case MR_ANGLE:
			QR_r = swapped.as_int16_t;
     			break;
		case LOG_MSG_PART:
	    		fprintf(log_file, "%i ", swapped.as_int16_t);
			break;
		case LOG_MSG_NEW_LINE:
			fprintf(log_file,"\n");
			break;
		case LOG_EVENT:
	    		fprintf(log_file_event, "%i ", swapped.as_int16_t);
			break;
		case LOG_EV_NEW_LINE:
			fprintf(log_file_event,"\n");
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

	if(fd_RS232>0){
		sendKeyData(ESCAPE);
  		close(fd_RS232);
	}
	endwin();
	fclose(log_file);
	fclose(log_file_event);
}
