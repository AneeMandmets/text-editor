/* INCLUDES */

#include <ctype.h>    // iscntrl()
#include <errno.h>    // EAGAIN, errno
#include <stdio.h>    // printf(), perror()
#include <stdlib.h>   // atexit(), exit()
#include <termios.h>  /* 
                        struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON, IEXTEN, 
                        ICRNL, OPOST, RKINT, INPCK, ISTRIP, CS8, VMIN, VTIME
                      */ 
#include <unistd.h>   // read(), STDIN_FILENO, write(), STDOUT_FILENO

/* DEFINES */

#define CTRL_KEY(k) ((k) & 0x1f) // Bitwise-ANDs the character with 00011111, in ASCII the control keys are 0-31

/* DATA */

struct termios orig_termios;

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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) unalive("tcsetattr");
}
 
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) unalive("tcgetattr");     // Read current terminal attributes into a struct
  atexit(disableRawMode);                                                     // Calls disableModeRaw on program exit
  struct termios raw = orig_termios;                  
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
char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) unalive("read");
  }
  return c;
}

/* OUTPUT */

void editorDrawRows() {
  int y;
  for (y = 0; y < 24; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); // Clear the screen, 4 is the amount of bytes we will be writing on the screen, \x1b is the escape character 
  write(STDOUT_FILENO, "\x1b[H", 3);  // Reposition the cursor at the top left of the screen, H is the cursor position command
  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/* INPUT */

// Waits for a keypress and processes it
void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      // Clear the screen
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      // Exit program  
      exit(0);
      break;
    default:
      break;
  }
}

/* INIT */

int main() {
  editorRefreshScreen();
  enableRawMode();

  while (1) {
    editorProcessKeypress();
  }    
  return 0; 
}
