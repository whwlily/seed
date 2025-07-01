
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

void die(const char *s);
void disableRawMode();;
void enableRawMode();