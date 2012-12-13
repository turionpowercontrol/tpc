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

bool Cpuid (DWORD fn, DWORD *eax, DWORD *ebx, DWORD *ecx, DWORD *edx);
bool ReadPciConfigDwordEx (DWORD devfunc, DWORD reg, DWORD *res);

bool WritePciConfigDwordEx (DWORD devfunc, DWORD reg, DWORD res) ;

bool RdmsrPx (DWORD msr,DWORD *eax,DWORD *ebx,PROCESSORMASK processor) ;
bool Rdmsr (DWORD msr,DWORD *eax,DWORD *ebx) ;

bool WrmsrPx (DWORD msr,DWORD eax,DWORD ebx,PROCESSORMASK processor);
bool Wrmsr (DWORD msr,DWORD eax,DWORD ebx);

void Sleep (DWORD ms);

int GetTickCount ();
