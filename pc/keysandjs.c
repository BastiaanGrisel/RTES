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
#include "queue.h"
#include "checksum.h"

#include <ncurses.h> /*for user key input*/

#define JS_DEV	"/dev/input/js0" /*define joystick location*/


#define SERIAL_DEVICE	"/dev/ttyS0"
#define USB_DEVICE0	"/dev/ttyUSB0" /* may need to change this */
#define USB_DEVICE1	"/dev/ttyUSB1"
#define WIFI_DEVICE 	"/dev/ttyUSB0" /* may need to change this */

#define LEFT_CHAR 'f'
#define RIGHT_CHAR 'h'
#define UP_CHAR 't'
#define DOWN_CHAR 'g'
#define QR_INPUT_BUFFERSIZE 1000
#define MESSAGESIZE 5

void init_keyboard(void);
struct timeval updateFPS(struct timeval oldtime);
int joystickInit(void);
void sendKeyData(int c);
void process_JS_event(int type, int number,int value);
void sendJSData(int number,int valueInt);
void printJSstate(void);

void rs232_open(void);
void rs232_close(void);
int rs232_putchar(char c);
int rs232_getchar_nb(void);
void parse_QR_input(char rec_c);
void print_QR_input(void);
void col_on(int col);
void col_off(int col);
void exitmain(void);

int fd_RS232, fd_js;

/* current axis and button readings
 */
int	axis[6];
int	button[12];
char received_chars[QR_INPUT_BUFFERSIZE];
int charpos = 0, loopcount = 0;
int	ae[4];
int offset[4];

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
	gettimeofday(&time,NULL);

	/* Main loop to process/send user input and to show QR input */
	while (1) {
		time = updateFPS(time);

		/* Check keypress */
		if ((c= getch()) != -1){
			sendKeyData(c); // send a message if user gave input
		} 

		/* Check joystick */
		if(fd_js>0){
			if (read(fd_js, &js, sizeof(struct js_event)) == 
				   			sizeof(struct js_event))  {
				process_JS_event(js.type,js.number,js.value);
			}
		}
		
		/* Check QR to pc communication */
		if(fd_RS232>0){
			while ((rec_c = rs232_getchar_nb())!= -1){
				parse_QR_input(rec_c);
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
void process_JS_event(int type, int number,int value){
	switch(type & ~JS_EVENT_INIT) {
		case JS_EVENT_BUTTON:
			button[number] = value;
			break;
		case JS_EVENT_AXIS:
			axis[number] = value/256;
			sendJSData(number,value);
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
	int fd,i;
	if ((fd = open(JS_DEV, O_RDONLY)) < 0) { // open connection
		/*endwin();
		perror("joystick /dev/input/js0 ");
		exit(1);*/
		printw("Joystick not found?! <<press a key to continue>>");
		nodelay(stdscr, false);
		getch();
		nodelay(stdscr, true);
		clear();
	} else {
		fcntl(fd, F_SETFL, O_NONBLOCK); // set joystick reading non-blocking
	}

	for (i = 0; i < 6; i++) {
		axis[i] = 0;
	}

	for (i = 0; i < 12; i++) {
		button[i]= 0;
	}

	return fd;
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
			rs232_putchar(control);
			rs232_putchar(value);
			rs232_putchar(packet_checksum(control,value));
			mvprintw(1,0,"sending: %c%i%i\n",control, (int) value, packet_checksum(control,value));
		}
		else{
			mvprintw(1,0,"NOT sending: %c%i   (RS232 = DISABLED)\n",control, (int) value);
		}
		
	} else {
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
			case 'l':
			case 'r':
				value = c;
				break;
			default:
				value = 0;
				break;
		}
		control = 'A'; // A == Adjust trimming
		if(fd_RS232>0 & value !=0){
			
			rs232_putchar(control);
			rs232_putchar(value);
			rs232_putchar(packet_checksum(control,value));
			mvprintw(1,0,"sending: %c%c%i\n",control, value, packet_checksum(control,value));
		}
		else{
			mvprintw(1,0,"NOT sending: %c%c %s\n",control, value,value==0?"key = not a control!":"(RS232 = DISABLED)");
		}
	}
}

/* Send Joystick data
 * Construct a message and send
 * Author: Henko Aantjes
 */
void sendJSData(int number,int valueInt){
	char control, value = (char)(valueInt/256);
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
		rs232_putchar(control);
		rs232_putchar(value);
		printw("sending: %c  %i (%i/256)\n",control, value, valueInt);
	}
	else{
		printw("NOT sending: %c  %c   (RS232 = DISABLED)\n",control, value);
	}
}

/* Parse QR message and 
 * Author: Henko Aantjes
 */
void parse_QR_message(int size){
	int message_number = 0;
	int mOffs = 0;
	while(size-mOffs>0){ // if size == 0 then 0%MESSAGESIZE will give a nasty exception
		if((size-mOffs)%MESSAGESIZE==0){ 
			
			//TODO check checksum
			switch(received_chars[0+mOffs]){
				case '\xFE':
					offset[0]=received_chars[1+mOffs];
					offset[1]=received_chars[2+mOffs];
					offset[2]=received_chars[3+mOffs];
					offset[3]=received_chars[4+mOffs];
					
				break; 
				case '\xFF':
					ae[0]=received_chars[1+mOffs];
					ae[1]=received_chars[2+mOffs];
					ae[2]=received_chars[3+mOffs];
					ae[3]=received_chars[4+mOffs];
					
				break; 
				default:
				break;
			
			}
			mOffs += MESSAGESIZE;
		} else {
			break;
		}
	}
}

/* Parse the QR input (one char at the time)
 * Call parse message if a message is complete
 * Author: Henko Aantjes
 */
void parse_QR_input(char rec_c){
	int i;
	int padding = 10;
	if(rec_c == '#'){
		//parse_QR_message(charpos);
		for(i = 0;i<100 & charpos<QR_INPUT_BUFFERSIZE;i++){
			received_chars[charpos++] = '\n';
		}

		charpos = 0;
		print_QR_input();
	} else{
		received_chars[charpos++] = rec_c;
	}
	if(charpos>=QR_INPUT_BUFFERSIZE){
		charpos = 0;
	}
}

/* Print messages that the QR has send to the pc
 * Author: Henko Aantjes
 */
void print_QR_input(void){
	// TODO print ae and offset and maybe more
	mvprintw(10,0,"received messages:(X32 -> pc) == {%s}\n", received_chars);
	//mvprintw(13,0,"received messages:(X32 -> pc) \n{%i,%i,%i,%i,%i,%i,%i,%i}\n", received_chars[0],received_chars[1],received_chars[2],received_chars[3],received_chars[4],received_chars[5],received_chars[6],received_chars[7]);
	//printw("#offset%i%i%i%i\n\n",offset[0]*10,offset[1]*10,offset[2]*10,offset[3]*10);
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
