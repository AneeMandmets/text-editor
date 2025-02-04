#include <ctype.h>    // iscntrl()
#include <stdio.h>    // printf()
#include <stdlib.h>   // atexit()
#include <termios.h>  /* 
                        struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON, IEXTEN, 
                        ICRNL, OPOST, RKINT, INPCK, ISTRIP, CS8, VMIN, VTIME
                      */ 
#include <unistd.h>   // read(), STDIN_FILENO

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
 
void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);                   // Read current terminal attributes into a struct
  atexit(disableRawMode);                                   // Calls disableModeRaw on program exit
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Fixes ctrl-m, Disables ctrl-s and ctrl-q
  raw.c_oflag &= ~(OPOST);                                  // Turns off output processing
  raw.c_cflag |= (CS8);                                     // Sets character size to 8 bits per byte
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // Modify the struct, Turn off canonical mode, disables ctrl-v, disables ctrl-c and ctrl-z
  raw.c_cc[VMIN] = 0;                                       // Sets minimum number of bytes needed before read() can return, 0 means it returns as soon as there is any input to be read
  raw.c_cc[VTIME] = 1;                                      // Sets maximum amount of time to wait before read() returns, 1/10 of a second
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);                 // Pass the modified struct to write the new terminal attributes back out
}

int main() {
  enableRawMode();

  while (1) {
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    if (iscntrl(c)) {     // Checks if the character is a control character, nonprintable characters
      printf("%d\r\n", c);  // Prints the ASCII value of the character
    } else {
      printf("%d ('%c')\r\n", c, c); // Prints the ASCII value and the character
    }
    if (c == 'q') break; // Breaks the loop if 'q' is entered
  }    
  return 0; 
}
