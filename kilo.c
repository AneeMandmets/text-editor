/* INCLUDES */

// Define feature test macros to enable certain features in the standard library
// These have to be before includes because they are used to enable certain features in the standard library
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>      // iscntrl()
#include <errno.h>      // EAGAIN, errno
#include <stdio.h>      // printf(), perror(), sscanf(), snprintf(), FILE, fopen(), getline(), fclose()
#include <stdlib.h>     // atexit(), exit(), realloc(), free(), malloc(),
#include <string.h>     // memcpy(), strlen()
#include <sys/ioctl.h>  // struct winsize, ioctl(), TIOCGWINSZ
#include <sys/types.h>  // ssize_t
#include <termios.h>    /* 
                          struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON, IEXTEN, 
                          ICRNL, OPOST, RKINT, INPCK, ISTRIP, CS8, VMIN, VTIME
                        */ 
#include <unistd.h>     // read(), STDIN_FILENO, write(), STDOUT_FILENO

/* DEFINES */

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) // Bitwise-ANDs the character with 00011111, in ASCII the control keys are 0-31

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,        // 1001
  ARROW_UP,           // 1002
  ARROW_DOWN,         // 1003
  DEL_KEY,            // 1004
  HOME_KEY,           // 1005
  END_KEY,            // 1006
  PAGE_UP,            // 1007
  PAGE_DOWN           // 1008
};

/* DATA */

typedef struct erow { // Editor row, stores a line of text as a pointer to the dynamically allocated character data and the length of the line
  int size;
  char *chars;
} erow;

struct editorConfig {
  int cx, cy;                     // Cursor x and y position
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  struct termios orig_termios;
};

struct editorConfig E; // Global variable containing our editor state

/* TERMINAL */

void unalive(const char *s) {
  // Clear the screen on exit
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  // Error message
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) unalive("tcsetattr");
}
 
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) unalive("tcgetattr");     // Read current terminal attributes into a struct
  atexit(disableRawMode);                                                     // Calls disableModeRaw on program exit
  struct termios raw = E.orig_termios;                  
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP);                   // Fixes ctrl-m, Disables ctrl-s and ctrl-q
  raw.c_iflag &= ~(IXON);
  raw.c_oflag &= ~(OPOST);                                                    // Turns off output processing
  raw.c_cflag |= (CS8);                                                       // Sets character size to 8 bits per byte
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);                            // Modify the struct, Turn off canonical mode, disables ctrl-v, disables ctrl-c and ctrl-z
  raw.c_cc[VMIN] = 0;                                                         // Sets minimum number of bytes needed before read() can return, 0 means it returns as soon as there is any input to be read
  raw.c_cc[VTIME] = 1;                                                        // Sets maximum amount of time to wait before read() returns, 1/10 of a second
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) unalive("tcsetattr");   // Pass the modified struct to write the new terminal attributes back out
}

// Wait for one keypress and return it
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) unalive("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1])
          {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O'){
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
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
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* ROW OPERATIONS */

void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/* FILE I/O */

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");    // Open the file in read mode
  if (!fp) unalive("fopen");          // If the file pointer is NULL, print an error message and exit  
  
  char *line = NULL;
  size_t linecap = 0;                 // line capacity
  ssize_t linelen;
  linelen = getline(&line, &linecap, fp);
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == '\r'))
      linelen--;
    
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

/* APPEND BUFFER */
// We create our own dynamic string type that supports appending

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0} // A macro that initializes an abuf struct, i.e an empty buffer

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

/* OUTPUT */

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y >= E.numrows) { // Is the row we are drawing part of the text buffer or does it come after it?
      if (E.numrows== 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;

        // --- Center the welcome message
        int padding = (E.screencols - welcomelen) / 2; // Divide screen width by 2 and subtract half of the length of the welcome message
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        // --- End of centering
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[y].size;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, E.row[y].chars, len);
    }
    abAppend(ab, "\x1b[K", 3); // Erase one line at a time
    if(y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);  // Hide the cursor, 6 is the amount of bytes we will be writing on the screen

  abAppend(&ab, "\x1b[H", 3);     // Reposition the cursor at the top left of the screen, H is the cursor position command
  editorDrawRows(&ab);

  // Moving the cursor
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1); // Move the cursor to the current position
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);  // Show the cursor
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/* INPUT */

// Allows the user move the cursor using the wasd keys
void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) { // If the cursor is not at the left edge of the screen
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) { // If the cursor is not at the right edge of the screen
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) { // If the cursor is not at the top edge of the screen
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) { // If the cursor is not at the bottom edge of the screen
        E.cy++;
      }
      break;
  }
}

// Waits for a keypress and processes it
void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      // Clear the screen
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      // Exit program  
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      E.cx = E.screencols - 1;
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

/* INIT */

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) unalive("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }    
  return 0; 
}
