
#include <term.h>
#include <curses.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "cpuPrimitives.h"

bool initializeCore()
{
	int fd;

	fd = open("/dev/cpu/0/cpuid", O_RDONLY);
	if (fd == -1) {
		printf("ERROR: couldn't open /dev/cpu/0/cpuid (%s).", strerror(errno));
		if (errno == ENXIO || errno == ENOENT) {
			printf(" Make sure that cpuid module is loaded.\n");
			return false;
		}
		if (errno == EACCES) {			
			printf(" Not root?.\n");
			return false;
		}
		printf("\n");
		return false;
	}
	close(fd);

	fd = open("/dev/cpu/0/msr", O_RDWR);
	if (fd == -1) {
		printf("ERROR: couldn't open /dev/cpu/0/msr (%s).", strerror(errno));
		if (errno == ENXIO || errno == ENOENT) {
			printf(" Make sure that msr module is loaded.\n");
			return false;
		}
		if (errno == EACCES) {			
			printf(" Not root?.\n");
			return false;
		}
		printf("\n");
		return false;
	}
	close(fd);

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

BOOL SysReadPciConfigDwordEx(DWORD pciAddress, DWORD regAddress, PDWORD value)
{
	return ReadPciConfigDwordEx(pciAddress, regAddress, value);
}

BOOL SysWritePciConfigDwordEx(DWORD pciAddress, DWORD regAddress, DWORD value)
{
	return WritePciConfigDwordEx(pciAddress, regAddress, value);
}
