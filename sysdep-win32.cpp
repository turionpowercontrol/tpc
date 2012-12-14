
#include <windows.h>
#include "sysdep.h"

void ClearScreen(unsigned int flags)
{
	HANDLE h;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD len;
	DWORD dummy;
	COORD corner = { 0, 0 };
	COORD now;

	h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h == INVALID_HANDLE_VALUE) {
		return;
	}
	
	if (!GetConsoleScreenBufferInfo(h, &csbi)) {
		return;
	}
	if (flags & CLEARSCREEN_FLAG_SMART) {
		len = csbi.dwSize.X * (csbi.dwCursorPosition.Y + 1);
	} else {
		len = csbi.dwSize.X * csbi.dwSize.Y;
	}

	if (!FillConsoleOutputCharacter(h, (TCHAR) ' ', len, corner, &dummy)) {
		return;
	}

	if (!FillConsoleOutputAttribute(h, csbi.wAttributes, len, corner, &dummy)) {
		return;
	}
	SetConsoleCursorPosition(h, corner);
}
