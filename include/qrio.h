#ifndef QRIO_H
#define QRIO_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <termios.h>
#include <assert.h>
#include <ncurses.h> /*for user key input*/

#include "times.h"

#define SERIAL_DEVICE	"/dev/ttyS0"
#define USB_DEVICE0	"/dev/ttyUSB0" /* may need to change this */
#define USB_DEVICE1	"/dev/ttyUSB1"
#define WIFI_DEVICE 	"/dev/ttyUSB0" /* may need to change this */

/* Open RS232 connection
 * Copy pasted by: Henko Aantjes
 */

extern int fd_RS232;

void rs232_open()
{
  	char 	 *name;
  	int 	 result;
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


/* Get a char from the RS232 (NON-Blocking)
 * copy pasted by: Henko Aantjes
 */
int	rs232_getchar_nb()
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
void	rs232_putchar(char c)
{
	int result;

	do {
		result = (int) write(fd_RS232, &c, sizeof(char));
	} while (result == 0);

	assert(result == 1);
	//return result;

	update_time();
}

#endif
