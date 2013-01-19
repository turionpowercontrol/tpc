
#include <term.h>
#include <curses.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>

bool initializeCore()
{
	return true;
}


bool deinitializeCore()
{
	return true;
}


void ClearScreen(unsigned int flags)
{
	static char *clearstr;

	if (!clearstr) {
		if (!cur_term) {
			int ret;

			if (setupterm(NULL, 1, &ret) == ERR) {
				return;
			}
		}
		clearstr = tigetstr("clear");
		if (!clearstr) {
			return;
		}
	}
	putp(clearstr);
}
