/*** includes ***/

#include <ctype.h>//iscntrl()
#include <errno.h>//errno, EAGAIN
#include <stdio.h>//printf(), perror()
#include <stdlib.h>//exit()
#include <termios.h> //struct termios, tcgetattr(), tcsetattr(), ECHO etc., TCSAFLUSH
#include <unistd.h> //read, STDIN_FILENO

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f) //macro to bitwise and the key entered with 00011111 which will take the 5 and 6 bits off (bits start at 0)this would change a (01100001) to 00000001 and A (01000001) to 00000001

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {//kills processes if there is an error
	perror(s);
	exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr"); //error handling
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode); //error handling

  struct termios raw = orig_termios; //make a copy of the original terminal scheme to work with 
	//bitwise and operations on each of the opposites of the settings bitmaps to reverse their selections ex. 0010 & 0010->1101 == 0000
	raw.c_iflag &= ~(IXON | ICRNL | INPCK | ISTRIP | BRKINT);//input flags
	raw.c_oflag &= ~(OPOST);//output flags
	raw.c_cflag |= (CS8);//control flags
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);//local flags
	raw.c_cc[VMIN] = 0;//minimum number of bytes to return
	raw.c_cc[VTIME] = 1;//maximum amount of time to wait before autoreturn in 1/10ths of a second. aka 100 milliseconds.
	
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** init ***/

int main() {
  enableRawMode();

	while (1) {
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");//in cygwin read outputs -1 like a failure and changes errno to EAGAIN when it returns instead of returning 0. but it also returns -1 on failure still. have to protect against this
		if (iscntrl(c)) {
			printf("%d\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);//carriage return and new line are both required to move the cursor down and left. we killed the auto return that normally comes with \n
		}
		if (c == CTRL_KEY('q')) break;//end program on ctrl+q
	}

  return 0;
}
