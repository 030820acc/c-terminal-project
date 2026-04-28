/*** includes ***/

#include <asm-generic/ioctls.h>
// #include <ctype.h>
#include <errno.h>//errno, EAGAIN
#include <stdio.h>//printf(), perror()
#include <stdlib.h>//exit()
#include <string.h>
#include <sys/ioctl.h>//ioctl, winsize
#include <termios.h> //struct termios, tcgetattr(), tcsetattr(), ECHO etc., TCSAFLUSH
#include <unistd.h> //read, STDIN_FILENO

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) //macro to bitwise and the key entered with 00011111 which will take the 5 and 6 bits off (bits start at 0)this would change a (01100001) to 00000001 and A (01000001) to 00000001

/*** data ***/

struct editorConfig {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {//sets errno to the string given and then kills the process with a 1 exit code for failure and the message
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr"); //error handling
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode); //error handling

  struct termios raw = E.orig_termios; //make a copy of the original terminal scheme to work with 
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

int getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
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

void editorDrawRows(struct abuf *ab) {//adds line markers
	int y;
	for (y = 0; y < E.screenrows; y++) {
		if (y == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen() {//clears screen and resets cursor
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** init ***/

void initEditor() {
	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
  enableRawMode();
	initEditor();

	while (1) {//on start clear screen and check for keypress and process
		editorRefreshScreen();
		editorProcessKeypress();
	}

  return 0;
}
