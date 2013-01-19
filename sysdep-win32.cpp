
#include <windows.h>
#include <stdio.h>
#include "OlsApi.h"
#include "OlsDef.h"
#include "sysdep.h"

bool initializeCore(void)
{
	int dllStatus;
	BYTE verMajor,verMinor,verRevision,verRelease;

	InitializeOls ();

	dllStatus=GetDllStatus ();
	
	if (dllStatus!=0) {
		printf ("Unable to initialize WinRing0 library\n");

		switch (dllStatus) {
			case OLS_DLL_UNSUPPORTED_PLATFORM:
				printf ("Error: unsupported platform\n");
				break;
			case OLS_DLL_DRIVER_NOT_LOADED:
				printf ("Error: driver not loaded\n");
				break;
			case OLS_DLL_DRIVER_NOT_FOUND:
				printf ("Error: driver not found\n");
				break;
			case OLS_DLL_DRIVER_UNLOADED:
				printf ("Error: driver unloaded by other process\n");
				break;
			case OLS_DLL_DRIVER_NOT_LOADED_ON_NETWORK:
				printf ("Error: driver not loaded from network\n");
				break;
			case OLS_DLL_UNKNOWN_ERROR:
				printf ("Error: unknown error\n");
				break;
			default:
				printf ("Error: unknown error\n");
		}

		return false;
	}

	GetDriverVersion (&verMajor,&verMinor,&verRevision,&verRelease);

	if ((verMajor>=1) && (verMinor>=2)) return true;

	return false;
}


bool deinitializeCore(void)
{
	DeinitializeOls();
	return true;
}


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
		len = csbi.dwSize.X * csbi.dwCursorPosition.Y + csbi.dwCursorPosition.X;
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
