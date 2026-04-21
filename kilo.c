/*** includes ***/

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

void die(const char *s) {//sets errno to the string given and then kills the process with a 1 exit code for failure and the message
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

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

char editorReadKey() {//reads key and returns it c
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

/*** input ***/

void editorProcessKeypress() {//checks for ctrl+q
	char c = editorReadKey();

	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** output ***/

void editorDrawRows() {
	int y;
	for (y = 0; y < 24; y++) {
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void editorRefreshScreen() {//clears screen and resets cursor
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	editorDrawRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

int main() {
  enableRawMode();

	while (1) {//on start clear screen and check for keypress and process
		editorRefreshScreen();
		editorProcessKeypress();
	}

  return 0;
}
