#include <sys/ioctl.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <assert.h>
#include <arpa/inet.h>

#include "joystick.h"
#include "checksum.h"
#include "types.h"
#include "communication.h"
#include "qrio.h"

//#include "logging.h"

#include <ncurses.h> /*for user key input*/

#define JS_DEV	"/dev/input/js0" /*define joystick location*/


#define SERIAL_DEVICE	"/dev/ttyS0"
#define USB_DEVICE0	"/dev/ttyUSB0" /* may need to change this */
#define USB_DEVICE1	"/dev/ttyUSB1"
#define WIFI_DEVICE 	"/dev/ttyUSB0" /* may need to change this */

#define QR_INPUT_BUFFERSIZE 1000
#define TIMEOUT 150 //ms

void init_keyboard(void);
struct timeval updateFPS(struct timeval oldtime);
int joystickInit(void);
void sendKeyData(int c);
void save_JS_event(int type, int number,int value);
struct timeval sendJSData(struct timeval packet_time);
void printJSstate(void);

void init_log(void);
void check_msg_q(void);
void col_on(int col);
void col_off(int col);
void exitmain(void);
void check_alive_connection();
void packet_received(char control, PacketData data);

int fd_RS232, fd_js;

/* current axis and button readings
 */
int	axis[6];
bool axisflags[6];
int	button[12];
FILE *log_file;
char received_chars[QR_INPUT_BUFFERSIZE];
int charpos = 0;
Fifo qr_msg_q;
int packet_counter=0;

int loopcount = 0; // to calculate the FPS
int	ae[4];
int offset[4];
int ms_last_packet_sent;
struct timeval keep_alive;

//************
int value_counter;
unsigned short int value_to_print;

/* Main function that mainly consists of polling the different connections
 * which are: Keyboard, Joystick, RS232 (connection to QR)
 */
int main (int argc, char **argv)
{
	struct js_event js;
	int i;
	int c, last_c;
	struct timeval time, last_packet_time;
	char rec_c;

	// init
	init_keyboard();
	init_joystick();
	rs232_open(fd_RS232);
	init_log();
	gettimeofday(&time,NULL);
	gettimeofday(&keep_alive,NULL);
	gettimeofday(&last_packet_time,NULL);

	/* Main loop to process/send user input and to show QR input */
	while (1) {
		time = updateFPS(time);
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

   		check_alive_connection();

		/* Check QR to pc communication */
		if(fd_RS232>0){
			while ((rec_c = rs232_getchar_nb(fd_RS232))!= -1){
				fifo_put(&qr_msg_q, rec_c);
				check_for_new_packets(&qr_msg_q, packet_received);
				mvprintw(9,0,"# packets: %i",packet_counter++);
			}
		}

		/* Print the Joystick state */
		printJSstate();

		/* Terminate program if user presses panic button or ESC */
		if (button[0] || c ==27){
			break;
		}
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
		mvprintw(0,0,"pc looptime: %i \tusec (%i \tHz) ",frametime,1000000/frametime);
		return timenew;
	} else {
		return timeold;
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

/* Print joystick state
 * Author: Henko Aantjes
 */
void printJSstate(void){
	int i;
	move(4,0);
	printw("Joystick axis: ");
	col_on(2);
	for (i = 0; i < 6; i++) {
		printw("%6d ",axis[i]);
	}
	col_off(2);
	printw("\nJoystick buttons: ");
	col_on(2);
	for (i = 0; i < 12; i++) {
		printw("%d ",button[i]);
	}
	col_off(2);
}

/* Initialize the key input
 *
 * Henko Aantjes
 */
void init_keyboard(void){
	setenv("ESCDELAY", "25", 0); // necessary to a fast detection of pressing esc (char == 27)
	initscr();
	keypad(stdscr, TRUE); // enable arrowkey-detection
	noecho(); // don't print what is typed
	cbreak(); // don't wait for an enter

	/* ******************************************************
	 * make a choice between one of the following 2 lines:  *
	 * halfdelay for demonstration pruposes or              *
	 * nodelay for real time programs                       *
	 ********************************************************/
    //halfdelay(1); // don't wait long for user input, give error instead
	nodelay(stdscr, TRUE); // don't wait for user input, give error instead
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_BLUE, COLOR_BLACK);
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
		/*endwin();
		perror("joystick /dev/input/js0 ");
		exit(1);*/
		printw("Joystick not found?! <<press a key to continue>>");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
		clear();
	} else {
		fcntl(fd_js, F_SETFL, O_NONBLOCK); // set joystick reading non-blocking
	}

	for (i = 0; i < 6; i++) {
		axis[i] = 0;
	}

	for (i = 0; i < 12; i++) {
		button[i]= 0;
	}

	return 0;
}


/* Checks if the PC has sent data in the last 200ms otherwise sends
a packet to keep alive the connection.
Author: Alessio */
void check_alive_connection() {

	gettimeofday(&keep_alive,NULL);
	int current_ms = ((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000);
	if(current_ms - ms_last_packet_sent > TIMEOUT)
	{
		send_control(rs232_putchar, 0);
	}
	return;
}


/* Send the user keyboard input
 * First construct the message and call the send function
 * Author: Henko Aantjes
 */
void sendKeyData(int c){
	char control, value =0; // the control and value to send
	if(c >= '0' && c<='5'){ //TODO mode change allowed?
		value = (char) c-'0';
		control = 'M';
		if(fd_RS232>0){
			send_char(rs232_putchar, control, value);
			//update the last packet timestamp
			mvprintw(1,0,"last mode message: %c%i{%i}\n",control, (int) value, checksum(control,ch2pd(value)));
		}
		else{
			mvprintw(1,0,"NOT sending: %c%i   (RS232 = DISABLED)\n",control, (int) value);
		}

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
			case 'a':
				value = LIFT_UP;
				break;
			case 'z':
				value = LIFT_DOWN;
				break;
			case 'q':
				value = YAW_LEFT;
				break;
			case 'w':
				value = YAW_RIGHT;
				break;
			case 'u':
				value = P_YAW_UP;
				break;
			case 'j':
				value = P_YAW_DOWN;
				break;
			case 'i':
				value = P_ROLL_UP;
				break;
			case 'k':
				value = P_ROLL_DOWN;
				break;
			case 'o':
				value = P_PITCH_UP;
				break;
			case 'l':
				value = P_PITCH_DOWN;
				break;
			case 'm':
				control = SPECIAL_REQUEST;
				value = ASK_MOTOR_RPM;
				break;
			case 'f':
				control = SPECIAL_REQUEST;
				value = ASK_FILTER_PARAM;
				break;
			case 'r':
				control = SPECIAL_REQUEST;
				value = RESET_MOTORS;
				break;
			case 's':
				control = SPECIAL_REQUEST;
				value = RESET_SENSOR_LOG;
				break;
			case 'x':
			  control = SPECIAL_REQUEST;
				value = ASK_SENSOR_LOG;
				break;
			case ESCAPE: // ESCAPEKEY
				control = SPECIAL_REQUEST;
				value = ESCAPE;
				break;
			default:
				value = 0;
				break;
		}

		if(fd_RS232>0 & value !=0){
			send_char(rs232_putchar, control, value);
			mvprintw(1,0,"last key message: %c%c{%i}   \n",control, value, checksum(control,ch2pd(value)));
		}
		else{
			mvprintw(1,0,"NOT sending: %c%c %s   \n",control, value,value==0?"key = not a control!":"(RS232 = DISABLED)");
		}
	}
}

/* Send Joystick data
 * Construct a message and send
 * Author: Henko Aantjes
 */
struct timeval sendJSData(struct timeval last_packet_time){
	struct timeval timenew;
	int number, control;
	PacketData data;

	for(number = 0; number < 4; number++){
		if(axisflags[number]) {
			switch(number) {
				case 0:
					control = JS_ROLL; // roll
					data.as_char = axis[number];
					break;
				case 1:
					control = JS_PITCH; // pitch
					data.as_char = axis[number];
					break;
				case 2:
					control = JS_YAW; // yaw
					data.as_char = axis[number];
					break;
				case 3:
					control = JS_LIFT; // throttle
					data.as_uint8_t = axis[number]; // Unsigned
					break;
				default:
					control = 0;
					break;
			}

			if(fd_RS232>0 & control !=0){
				gettimeofday(&timenew,NULL);
				int timediff = timenew.tv_usec+1000000*timenew.tv_sec-last_packet_time.tv_usec-1000000*last_packet_time.tv_sec;

				if(timediff < 1000*10){
					mvprintw(2,0,"timediff is to low: ", timediff);
					return last_packet_time;
				} else {
					axisflags[number] = false;
					send_packet(rs232_putchar, control, data);
					mvprintw(1,0,"last JS message: %c  %i (%i/256)\n",control, axis[number], axis[number]*256);
					return timenew;
				}
			} else {
				mvprintw(1,0,"NOT sending: %c  %c   (RS232 = DISABLED)\n",control, axis[number]);
			}
		}
	}
}



/* initialize the log file
 * Author: Henko Aantjes
 */
void init_log(void){
	value_counter = 0;
	value_to_print = 0;

	log_file = fopen("flight_log.txt", "w");
	if (log_file == NULL)
	{
		printw("Error opening file!\n");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
	}
}

/* Prints the value to the log file with #awesomesauce
   Author: Alessio
*/
/*void print_value_to_file(unsigned char c)
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
} */


/*Swap the endianess.
Author: Alessio*/
uint16_t swap_endianess_16(uint16_t val)
{
	return (val >> 8) | (val << 8 );
}

/*Print log values to file, taking in account the endianess
Author: Alessio */
void print_log_to_file(PacketData data)
{
	uint16_t val = data.as_uint16_t;
	val = swap_endianess_16(val);

  //provisional workaround
	val = (val == 32000) ? 255 : val;

	fprintf(log_file, "%hu ", val);
}


/* Parse the QR input (one char at the time)
 * Call parse message if a message is complete
 * Author: Henko Aantjes
 */
void packet_received(char control, PacketData data){
	int i;
	char value = data.as_bytes[0];
	uint16_t val;

	if(control == 0) return;
	
	switch(control){
		case TERMINAL_MSG_START: // start new qr terminal message
			charpos = 0;
			break;
		case TERMINAL_MSG_PART: // characters of the terminal message
			if(charpos<QR_INPUT_BUFFERSIZE)
				received_chars[charpos++]= value;
			break;
		case TERMINAL_MSG_FINISH:
			// print the terminal message
			col_on(3);
			mvprintw(10, 0, "received messages: (X32 -> pc) == {%.*s}         \n\n\n\n", charpos, received_chars);
			col_off(3);
			break;
		case LOG_MSG_PART:
	//		print_value_to_file((unsigned char) value);
	    print_log_to_file(data);
			break;
		case LOG_MSG_NEW_LINE:
			fprintf(log_file,"\n");
			break;
		case ERROR_MSG:
		  val = data.as_uint16_t;
		  parse_error_message(swap_endianess_16(val));
			break;
		default:
			break;
	}
}

/*Displays the error message.
Author: Alessio */
parse_error_message(Error err)
{
	char msg[50];
	col_on(1);
	switch(err) {
		case LOG_ONLY_IN_SAFE_MODE:
			sprintf(msg,"[QR]: Switch to SAFE before asking for the logging.]");
			break;
		case MODE_ILLIGAL:
		  sprintf(msg,"[QR]: Invalid or illigal mode.]");
			break;
	  case MODE_CHANGE_ONLY_VIA_SAFE:
		  sprintf(msg, "[QR]: Mode can be changed only from SAFE mode.");
			break;
		case MODE_CHANGE_ONLY_IF_ZERO_RPM:
			sprintf(msg, "[QR]: Cannot change mode. RPM are not zero.");
			send_char(rs232_putchar, SPECIAL_REQUEST,ASK_MOTOR_RPM); //when RPM will be visualized in real-time, this won't be needed
			break;
		case MODE_ALREADY_SET:
			sprintf(msg, "[QR]: Mode already changed.");
			break;
		case CONTROL_DISABLED_IN_THIS_MODE:
			sprintf(msg, "[QR]: Manual control disabled in this mode. ");
			break;
		default:
		 sprintf(msg, "[QR] Wrong not recognized. Wrong error code.");
	}

	mvprintw (18,0,"%.*s \n\n",50,msg);
	col_off(1);
}

/* Exit routine
 * Author: Henko Aantjes
 */
void exitmain(void){
	if(fd_RS232>0){
  		close(fd_RS232);
	}
	endwin();
	fclose(log_file);
}
