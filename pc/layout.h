/* Author: Bastiaan */
#ifndef LAYOUT_H
#define LAYOUT_H

/* Change color of terminal output */
void col_on(int col){
	attron(COLOR_PAIR(col));
}
/* reset color of terminal output */
void col_off(int col){
	attroff(COLOR_PAIR(col));
}

void init_colors() {
	start_color();
	init_pair(0, COLOR_WHITE, COLOR_BLACK);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
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
}

void drawMode(Mode m) {
	ptb(1,20,"%-20s",mode_to_string(m));
}

void drawJS(int R, int P, int Y, int T) {
	ptbc(3,20,2,"%-4d",R);
	ptbc(4,20,2,"%-4d",P);
	ptbc(5,20,2,"%-4d",Y);
	ptbc(6,20,2,"%-4d",T);
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

#endif
