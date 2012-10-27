
#include <term.h>
#include <curses.h>
#include <unistd.h>

void ClearScreen(unsigned int flags)
{
	if (!cur_term) {
		int ret;

		if (setupterm(NULL, 1, &ret) == ERR) {
			return;
		}
	}
	putp(tigetstr("clear"));
}
