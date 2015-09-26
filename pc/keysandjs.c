#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <assert.h>

#include "joystick.h"
#include "fifo.h"
#include "checksum.h"
#include "types.h"

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
void sendJSData();
void printJSstate(void);
void send_message(char control, char value);

void init_log(void);
void rs232_open(void);
void rs232_close(void);
int rs232_putchar(char c);
int rs232_getchar_nb(void);
void check_msg_q(void);
void col_on(int col);
void col_off(int col);
void exitmain(void);
void check_alive_connection();

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
int value_counter = 0;
int value_to_print = 0;
int new_line_counter = 0;

/* Main function that mainly consists of polling the different connections
 * which are: Keyboard, Joystick, RS232 (connection to QR)
 */
int main (int argc, char **argv)
{
	struct js_event js;
	int i;
	int c, last_c;
	struct timeval time;
	char rec_c;

	// init

	init_keyboard();
	init_joystick();
	rs232_open();
	init_log();
	gettimeofday(&time,NULL);
	gettimeofday(&keep_alive,NULL);

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
			sendJSData();
		}

   		check_alive_connection();

		/* Check QR to pc communication */
		if(fd_RS232>0){
			while ((rec_c = rs232_getchar_nb())!= -1){
				fifo_put(&qr_msg_q, rec_c);
				check_msg_q();
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
void check_alive_connection()
{
 /*	struct timeval current_time;
	gettimeofday(&current_time,NULL);*/
	gettimeofday(&keep_alive,NULL);
	int current_ms = ((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000);
	if(current_ms - ms_last_packet_sent > TIMEOUT)
	{
		send_message(0,0);
	}
	return;
}

/*Update the ms_last_packet_sent variable
Author: Alessio*/
void update_time()
{
	/* struct timeval current_time;
	gettimeofday(&current_time,NULL);*/
	gettimeofday(&keep_alive,NULL);
	ms_last_packet_sent = ((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000);
}


/* Send the user keyboard input
 * First construct the message and call the send function
 * Author: Henko Aantjes
 */
void sendKeyData(int c){
	char control, value; // the control and value to send
	if(c >= '0' && c<='5'){
		value = (char) c-'0';
		control = 'M';
		if(fd_RS232>0){
			send_message(control, value);
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
				value = LEFT_CHAR;
				break;
			case KEY_RIGHT:
				value = RIGHT_CHAR;
				break;
			case KEY_UP:
				value = UP_CHAR;
				break;
			case KEY_DOWN:
				value = DOWN_CHAR;
				break;
			case 27: // ESCAPE
			case 'a':
			case 'z':
			case 'q':
			case 'w':
			case 'u':
			case 'j':
			case 'i':
			case 'k':
			case 'o':
			case 'm': // get motor values
			case 'f': // get filter values
			case 'r':
				value = c;
				break;
			case 'l':
				control = 'L';
				value = c;
				break;
			default:
				value = 0;
				break;
		}

		if(fd_RS232>0 & value !=0){
			send_message(control, value);
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
void sendJSData(){
	int number, control, value;
	for(number=0;number<4;number++){
		if(axisflags[number]){
			axisflags[number] = false;
			switch(number){
				case 0:
					control = 'R'; // roll
					break;
				case 1:
					control = 'P'; // pitch
					break;
				case 2:
					control = 'Y'; // yaw
					break;
				case 3:
					control = 'T'; // throttle
					break;
				default:
					control = 0;
					break;
			}
			if(fd_RS232>0 & control !=0){
				send_message(control, axis[number]);
				mvprintw(1,0,"last JS message: %c  %i (%i/256)\n",control, axis[number], axis[number]*256);
			}
			else{
				mvprintw(1,0,"NOT sending: %c  %c   (RS232 = DISABLED)\n",control, value);
			}
		}
	}
}

/* send a complete message
 * Author: Henko Aantjes
 */
void send_message(char control, char value){
	PacketData p;
	p.bytes[0] = value;
	
	rs232_putchar(control);
	rs232_putchar(p.bytes[0]);
	rs232_putchar(p.bytes[1]);
	rs232_putchar(checksum(control,p));
	update_time();
}

/* initialize the log file
 * Author: Henko Aantjes
 */
void init_log(void){
	int value_counter = 0;
	int value_to_print = 0;
	int new_line_counter = 0;

	log_file = fopen("flight_log.txt", "w");
	if (log_file == NULL)
	{
		printw("Error opening file!\n");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
	}
}

//Prints the value to the log file with #awesomesauce
void print_value_to_file(char c)
{
	 if(c == '~') { //this is the value sent by send_logs() at the end of every for-cycle
		 fprintf(log_file,"\n");
		 return;
	 }

   //even: read most significant bytes
	 if(value_counter % 2 == 0) {
		 value_to_print = c;
		 value_to_print = (value_to_print << 8) & 0xFF00;
	 }

  //odd: read least signifative bytes and print the value
	 else {
		 value_to_print |= c;
	/*	 gettimeofday(&keep_alive,NULL);
	/	 if((((keep_alive.tv_usec+1000000*keep_alive.tv_sec) / 1000) % 10) == 0) printw(" rec = %i \n\n",c);*/

		 fprintf(log_file,"%i ",value_to_print);
		// if(value_counter % 14 == 1) fprintf(log_file, ": ");
	 }

	// value_to_print = (value_counter % 2 == 0) ? ((c << 8) & 0xFF00) : (value_to_print | c);
	 value_counter++;
}

/* Print a char to a file
 * Author: Henko Aantjes
 */
void print_char_to_file(char c){
	fprintf(log_file,"%i ",c);
}

/* Parse the QR input (one char at the time)
 * Call parse message if a message is complete
 * Author: Henko Aantjes
 */
void packet_received(char control, PacketData data){
	int i;
	char value = data.bytes[0];
	
	switch(control){
		case 'B': // start new qr terminal message
			charpos = 0;
			break;
		case 'T': // characters of the terminal message
			if(charpos < QR_INPUT_BUFFERSIZE)
				received_chars[charpos++]= value;
			break;
		case 'F':
			// print the terminal message
			mvprintw(10, 0, "received messages: (X32 -> pc) == {%.*s}         \n\n\n\n", charpos, received_chars);
			break;
		case 'L':
		  	print_value_to_file(value);
			break;
		default:
			break;
	}

}

void check_msg_q(){
	while(fifo_size(&qr_msg_q) >= 4) { // Check if there are one or more packets in the queue
		char control;
		PacketData data;
		char checksum;

		fifo_peek_at(&qr_msg_q, &control, 0);
		fifo_peek_at(&qr_msg_q, &data.bytes[0], 1);
		fifo_peek_at(&qr_msg_q, &data.bytes[1], 2);
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


/* Open RS232 connection
 * Copy pasted by: Henko Aantjes
 */
void rs232_open(void)
{
  	char 		*name;
  	int 		result;
  	struct termios	tty;
  	int serial_device = 1;


	if (serial_device == 0)
	{
		fd_RS232 = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
		fprintf(stderr,"using /dev/ttyS0\n");

	}
	else if ( (serial_device == 1) || (serial_device == 2) )
	{
        	fd_RS232 = open(USB_DEVICE0, O_RDWR | O_NOCTTY);
			//printw("fd usb0 = %i\n",fd_RS232);
		fprintf(stderr,"using /dev/ttyUSB0\n");
		if(fd_RS232<0){ // try other name
			fd_RS232 = open(USB_DEVICE1, O_RDWR | O_NOCTTY);
			//printw("fd usb1 = %i\n",fd_RS232);
		}
	}

  	if(isatty(fd_RS232)<0 | ttyname(fd_RS232) ==0| tcgetattr(fd_RS232, &tty)!=0){

		printw("RS232 not found?!     <<press a key to continue>>\nfd = %i;isatty(fd_RS232)= %i;ttyname(fd_RS232) = %i; tcgetattr(fd_RS232, &tty) = %i",fd_RS232,isatty(fd_RS232),ttyname(fd_RS232),tcgetattr(fd_RS232, &tty));
		fd_RS232 = -1;
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
		clear();
	} else{
		tty.c_iflag = IGNBRK; /* ignore break condition */
		tty.c_oflag = 0;
		tty.c_lflag = 0;

		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; /* 8 bits-per-character */
		tty.c_cflag |= CLOCAL | CREAD; /* Ignore model status + read input */

		/* Set output and input baud rates.
		 */
		if (serial_device == 0 || serial_device == 1) // wired
		{
			cfsetospeed(&tty, B115200);
			cfsetispeed(&tty, B115200);
		}
			else if (serial_device == 2) // wireless
		{
			cfsetospeed(&tty, B9600);
			cfsetispeed(&tty, B9600);
		}

		tty.c_cc[VMIN]  = 0;
		tty.c_cc[VTIME] = 0;

		tty.c_iflag &= ~(IXON|IXOFF|IXANY);

		result = tcsetattr (fd_RS232, TCSANOW, &tty); /* non-canonical */

		tcflush(fd_RS232, TCIOFLUSH); /* flush I/O buffer */
	}
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

/* Get a char from the RS232 (NON-Blocking)
 * copy pasted by: Henko Aantjes
 */
int	rs232_getchar_nb(void)
{
	int 		result;
	unsigned char 	c;

	result = read(fd_RS232, &c, 1);

	if (result == 0) {
		return -1;
	} else {
		assert(result == 1);
		return (int) c;
	}
}

/* Send a char over the RS232 to the pc (Blocking)
 * copy pasted by: Henko Aantjes
 */
int	rs232_putchar(char c)
{
	int result;

	do {
		result = (int) write(fd_RS232, &c, sizeof(char));
	} while (result == 0);

	assert(result == 1);
	return result;
}
