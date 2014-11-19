#include <inttypes.h>
#include <sys/types.h>
#include "Processor.h"

#define DWORD uint32_t
#define PDWORD DWORD*

#ifdef __LP64__
	#define DWORD_PTR uint64_t
#else
	#define DWORD_PTR uint32_t
#endif

#define TRUE true
#define FALSE false
#define BOOL bool

BOOL Cpuid(DWORD index, PDWORD eax, PDWORD ebx, PDWORD ecx, PDWORD edx);

BOOL ReadPciConfigDwordEx(DWORD pciAddress, DWORD regAddress, PDWORD value);
BOOL WritePciConfigDwordEx(DWORD pciAddress, DWORD regAddress, DWORD value);

BOOL RdmsrPx(DWORD index, PDWORD eax, PDWORD edx, DWORD_PTR processAffinityMask);
BOOL Rdmsr(DWORD index, PDWORD eax, PDWORD edx);

BOOL WrmsrPx(DWORD index, DWORD eax, DWORD edx, DWORD_PTR processorAffinityMask);
BOOL Wrmsr(DWORD index, DWORD eax, DWORD edx);

void Sleep (DWORD ms);

int GetTickCount ();
