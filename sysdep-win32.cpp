
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

//
// Tests if special PCI Extended Configuration Space access is required.
//
// This is true on Windows XP/2003 and configuration space offsets >= 0x100
// as these operating systems do not provide appropriate API.
// 
// More info:
//   http://www.osronline.com/showThread.cfm?link=87866
//   https://code.google.com/p/turionpowercontrol/issues/detail?id=28
//
bool SpecialEcsAccessRequired(DWORD reg)
{
	OSVERSIONINFO versioninfo;
	if (reg < 0x100)
		return false;
	versioninfo.dwOSVersionInfoSize = sizeof(versioninfo);
	if (!GetVersionEx(&versioninfo)) {
		return false;
	}
	if (versioninfo.dwMajorVersion >= 6) {
		return false;
	}
	return true;
}

//
// Performs special Extended Configuration Space access.
//
// It takes advantage of AMD-specific IO CF8h/CFCh extension that enables
// accesses to PCI Extended Configuration Space. Access is realized by
// enabling ECS access via IOCF8/IOCFC using EnableCf8ExtCfg from NB_CFG
// MSR (MSRC001_001F) and performing CF8h/CFCh access per specifciation.
// State of NB_CFG and IOCF8 is saved before the operation and gets
// restored afterwards. Calling process is also bound to first CPU
// in the system for the duration of the operation to accommodate
// environments with multiple northbridges.
// 
// EnableCf8ExtCfg is supported by all currently supported CPU families,
// that is, 10h, 11h, 12h, 14h, 15h and 16h.
//
// IO CF8h/CFCh method, while racy, is the only feasible method to access
// PCI ECS on Windows XP/2003 (accessing memory mapped configuration space
// is off the table due to lack of physical memory access support
// in WinRing0).
//
// Best effort is put to detect simultaneous users of CF8h/CFCh.
//
bool SpecialEcsAccess(bool write, DWORD devfunc, DWORD reg, DWORD *res)
{
	DWORD_PTR mask_save;
	DWORD_PTR dummy;
	DWORD eax_save;
	DWORD edx_save;
	DWORD edx;
	DWORD addr;
	DWORD addr_save;
	DWORD addr_check;
	DWORD data;
	bool result = false;

	if (!GetProcessAffinityMask((HANDLE)-1, &mask_save, &dummy)) {
		fprintf(stderr, "ERROR getting affinity mask\n");
		goto out;
	}
	if (!SetProcessAffinityMask((HANDLE)-1, 1)) {
		fprintf(stderr, "ERROR setting affinity mask\n");
		goto out;
	}
	if (!RdmsrPx(0xC001001F, &eax_save, &edx_save, 1)) {
		fprintf(stderr, "ERROR reading NB_CFG\n");
		goto out_affinity;
	}
	edx = edx_save;
	edx |= 0x4000;
	if (!WrmsrPx(0xC001001F, eax_save, edx, (DWORD_PTR)1)) {
		fprintf(stderr, "ERROR writing NB_CFG\n");
		goto out_affinity;
	}
	addr_save = ReadIoPortDword(0xcf8);
	addr = 1;
	addr <<= 7;
	addr |= (reg >> 8) & 0x0F;
	addr <<= 16;
	addr |= devfunc & 0xFFFF;
	addr <<= 8;
	addr |= reg & 0xFF;
	WriteIoPortDword(0xcf8, addr);
	if (write == false) {
		data = ReadIoPortDword(0xcfc);
	} else {
		WriteIoPortDword(0xcfc, *res);
	}
	addr_check = ReadIoPortDword(0xcf8);
	if (addr_check != addr) {
		fprintf(stderr, "ERROR: IO CF8h hijacked!\n");
		goto out_nbcfg;
	}
	WriteIoPortDword(0xcf8, addr_save);
	if (write == false) {
		*res = data;
	}
	result = true;
out_nbcfg:
	WrmsrPx(0xC001001F, eax_save, edx_save, (DWORD_PTR)1);
out_affinity:
	SetProcessAffinityMask((HANDLE)-1, mask_save);
out:
	return result;
}

BOOL SysReadPciConfigDwordEx(DWORD pciAddress, DWORD regAddress, PDWORD value)
{
	if (SpecialEcsAccessRequired(regAddress)) {
	        return SpecialEcsAccess(false, pciAddress, regAddress, value);
	}
	return ReadPciConfigDwordEx(pciAddress, regAddress, value);
}

BOOL SysWritePciConfigDwordEx(DWORD pciAddress, DWORD regAddress, DWORD value)
{
	if (SpecialEcsAccessRequired(regAddress)) {
	        return SpecialEcsAccess(true, pciAddress, regAddress, &value);
	}
	return WritePciConfigDwordEx(pciAddress, regAddress, value);
}
