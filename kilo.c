#include <ctype.h>    // iscntrl()
#include <stdio.h>    // printf()
#include <stdlib.h>   // atexit()
#include <termios.h>  // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON, IEXTEN, ICRNL
#include <unistd.h>   // read(), STDIN_FILENO

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
 
void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);           // Read current terminal attributes into a struct
  atexit(disableRawMode);                           // Calls disableModeRaw on program exit
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON);                   // Fixes ctrl-m, Disables ctrl-s and ctrl-q
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  // Modify the struct, Turn off canonical mode, disables ctrl-v, disables ctrl-c and ctrl-z
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);         // Pass the modified struct to write the new terminal attributes back out
}

int main() {
  enableRawMode();    

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
  {
    if (iscntrl(c)) {     // Checks if the character is a control character, nonprintable characters
      printf("%d\n", c);  // Prints the ASCII value of the character
    } else {
      printf("%d ('%c')\n", c, c); // Prints the ASCII value and the character
    }
  }
  

  while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q'); // Reads input until file end or 'q' gets entered
  return 0; 
}
