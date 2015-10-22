#ifndef KEYSANDJS_H
#define KEYSANDJS_H

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
#include "PCmessage.h"

#include <ncurses.h> /*for user key input*/

#define JS_DEV	"/dev/input/js0" /*define joystick location*/


#define SERIAL_DEVICE	"/dev/ttyS0"
#define USB_DEVICE0	"/dev/ttyUSB0" /* may need to change this */
#define USB_DEVICE1	"/dev/ttyUSB1"
#define WIFI_DEVICE 	"/dev/ttyUSB0" /* may need to change this */

#define QR_INPUT_BUFFERSIZE 1000
#define TIMEOUT 150 //ms
#define MAX_MSG_TIME 200000 //frames
#define MAX_ERROR_MSG_TIME 200000 //frames
//Number of line where to print in the window
#define LINE_NR_FPS 0
#define LINE_NR_JS_STATE 4
#define LINE_NR_RECEIVED_MSG 10
#define LINE_NR_ERROR_MSG 14
#define LINE_NR_QR_STATE 16
#define DRONE_COL 70
#define DRONE_LN 12


void init_keyboard(void);
struct timeval updateFPS(struct timeval oldtime);
int joystickInit(void);
void sendKeyData(int c);
void processMouse(int button, int line, int x);
void save_JS_event(int type, int number,int value);
struct timeval sendJSData(struct timeval packet_time);
void printJSstate(void);
void printQRstate(void);
void clearMessages(void);

void init_log(void);
void check_msg_q(void);
void col_on(int col);
void col_off(int col);
void exitmain(void);
void check_alive_connection();

void drawBase();
void drawJS(int R, int P, int Y, int T);
void drawMode(Mode m);
void drawSensors(int sensors[6]);
void drawAngles(int R, int P);
void drawControl(int R_s, int P_s, int Y_s);
void drawCommunication(int in_packets, int out_packets);
void displayMessage(char* message, char* error, char* terminal_message);
void drawQR(int col,int line);
void drawRPM(int m0, int m1, int m2, int m3);

#endif
