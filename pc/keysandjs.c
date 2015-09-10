
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "joystick.h"

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


#define JS_ENABLED 1
#define RS232_enabled 1

void keyInit(void);
int joystickInit(void);
void sendKeyData(int c);
void sendJSData(int number,int valueInt);
void rs232_open(void);
void rs232_close(void);
int rs232_putchar(char c);
int	rs232_getchar_nb(void);

int fd_RS232;

/* current axis and button readings
 */
int	axis[6];
int	button[12];


int main (int argc, char **argv)
{
	int fd;
	struct js_event js;
	unsigned int i;
	int c, last_c;
	char rec_c;
	char received_chars[1000];
	int charpos = 0;

    // init keyspress functionality
    keyInit();
	printw("start up screen\n");

	if(JS_ENABLED){
    	// init joystick functionality 
    	fd = joystickInit();
    }
    if(RS232_enabled){
    	// open connection to X32
    	rs232_open();
    }

	while (1) {
		printw("waiting for char\n");
	    /* check keypress */
	    c = getch();
		printw("received a char\n");
	    if (c != -1){
	        last_c = c;
	    }
	    clear();
			printw("start up screen3\n");
		if (c != -1){
			sendKeyData(c); // send a message if user gave input
	    } else{
	    	printw("NOTHING TO SEND\n");
	    }
	    
		if(JS_ENABLED){
		/* check joystick */
			if (read(fd, &js, sizeof(struct js_event)) == 
				   			sizeof(struct js_event))  {

				/* register data
				 */
				// fprintw(stderr,".");
				switch(js.type & ~JS_EVENT_INIT) {
					case JS_EVENT_BUTTON:
						button[js.number] = js.value;
						break;
					case JS_EVENT_AXIS:
						axis[js.number] = js.value;
						sendJSData(js.number,js.value);
						break;
				}
			}
		}
		
		if(RS232_enabled){
			while ((rec_c = rs232_getchar_nb())!= -1){
				received_chars[charpos++] = rec_c;
				if(charpos>=1000 | rec_c == '#'){
					charpos = 0;
				}
			}
		}
		
		/*if (errno != EAGAIN) {
		    endwin();
			perror("\njs: error reading (EAGAIN)");
			// this error might be thrown because I changed the while(read) to if(read)
			exit (1);
		}*/
		

		printw("\n");
		printw("keyCode: \'%c\', last valid: \'%c\' \n",c,last_c);
		
		printw("Joystick axis: ");
		for (i = 0; i < 6; i++) {
			printw("%6d ",axis[i]);
		}
		printw("\nJoystick buttons: ");
		for (i = 0; i < 12; i++) {
			printw("%d ",button[i]);
		}
		printw("\n\nreceived messages: \n%s\n", received_chars);
		
		// terminate program if user presses panic button or ESC
		if (button[0] || c ==27){
			endwin();
			rs232_close();
			//printf("Having fun?!\n");
			return 0;
			
		}
	}
	
	endwin();
	return 0;

}

/* initialize the key input
 *  
 * Henko Aantjes
 */
void keyInit(void){
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
    halfdelay(1); // don't wait long for user input, give error instead	
	//nodelay(stdscr, TRUE); // don't wait for user input, give error instead
}

/* Initialize joystick connection
 * contributing author: Henko
 */
int joystickInit(void){
    int fd,i;
    if ((fd = open(JS_DEV, O_RDONLY)) < 0) { // open connection
		endwin();
		perror("joystick /dev/input/js0 ");
		exit(1);
	}
	fcntl(fd, F_SETFL, O_NONBLOCK); // set joystick reading non-blocking
	
	for (i = 0; i < 6; i++) {
		axis[i] = 0;
	}

	for (i = 0; i < 12; i++) {
		button[i]= 0;
	}
	
	
	return fd;
	
}
void sendKeyData(int c){
	char control, value; // the control and value to send
	if(c >= '0' && c<='5'){
		value = (char) c-'0';
		control = 'M';
		if(RS232_enabled){
			
			rs232_putchar(control);
			rs232_putchar(value);
			printw("sending: %c%i\n",control, (int) value);
		}
		else{
			printw("NOT sending: %c%i   (RS232 = DISABLED)\n",control, (int) value);
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
				value = c;
				break;
			default:
				value = 0;
				break;
		}
		control = 'A'; // A == Adjust trimming
		if(RS232_enabled & value !=0){
			
			rs232_putchar(control);
			rs232_putchar(value);
			printw("sending: %c  %c\n",control, value);
		}
		else{
			printw("NOT sending: %c  %c   (RS232 = DISABLED)\n",control, value);
		}
	}
}

/* Send Joystick data
 *
 * Author: Henko Aantjes
 */
void sendJSData(int number,int valueInt){
	char control, value = (char)(valueInt<<8);
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
	if(RS232_enabled & control !=0){
		rs232_putchar(control);
		rs232_putchar(value);
		printw("sending: %c  %c\n",control, value);
	}
	else{
		printw("NOT sending: %c  %c   (RS232 = DISABLED)\n",control, value);
	}
}

#include <termios.h>
#include <assert.h>
/* Open RS232 connection
 * copy pasted by: Henko Aantjes
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
		fprintf(stderr,"using /dev/ttyUSB0\n"); 
		if(fd_RS232<0){
			fd_RS232 = open(USB_DEVICE1, O_RDWR | O_NOCTTY);
		}
	} 

	assert(fd_RS232>=0);

  	result = isatty(fd_RS232);
  	assert(result == 1);

  	name = ttyname(fd_RS232);
  	assert(name != 0);

  	result = tcgetattr(fd_RS232, &tty);	
	assert(result == 0);

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

void 	rs232_close(void)
{
  	int 	result;

  	result = close(fd_RS232);
  	assert (result==0);
}

int	rs232_getchar_nb(void)
{
	int 		result;
	unsigned char 	c;

	result = read(fd_RS232, &c, 1);

	if (result == 0) 
		return -1;
	
	else 
	{
		assert(result == 1);   
		return (int) c;
	}
}


/*int 	rs232_getchar()
{
	int 	c;

	while ((c = rs232_getchar_nb()) == -1) 
		;
	return c;
}*/


int 	rs232_putchar(char c)
{ 
	int result;

	do {
		result = (int) write(fd_RS232, &c, sizeof(char));
	} while (result == 0);   

	assert(result == 1);
	return result;
}
