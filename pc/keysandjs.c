
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

#include <ncurses.h>

void keyInit(void);
int joystickInit(void);


/* current axis and button readings
 */
int	axis[6];
int	button[12];


#define JS_DEV	"/dev/input/js0"

int main (int argc, char **argv)
{
	int fd;
	struct js_event js;
	unsigned int i;
	int c, last_c;

    // init keyspress functionality
    keyInit();

    // init joystick functionality 
    fd = joystickInit();

	while (1) {
	
	    /* check keypress */
	    c = getch();
	    if (c != -1){
	        last_c = c;
	    }
	    
	
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
					break;
			}
		}
		/*if (errno != EAGAIN) {
		    endwin();
			perror("\njs: error reading (EAGAIN)");
			// this error might be thrown because I changed the while(read) to if(read)
			exit (1);
		}*/

		printw("\n");
		printw("keyCode: %5i, last valid: %5i | ",c,last_c);
		
		for (i = 0; i < 6; i++) {
			printw("%6d ",axis[i]);
		}
		printw(" |  ");
		for (i = 0; i < 12; i++) {
			printw("%d ",button[i]);
		}
		if (button[0] || c ==27)
			break;
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
