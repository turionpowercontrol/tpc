#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include "cpuPrimitives.h"
#include <time.h>

#define DWORD uint32_t
#define TRUE true
#define FALSE false

BOOL Cpuid(DWORD index, PDWORD eax, PDWORD ebx, PDWORD ecx, PDWORD edx)
{
	char cpuid_filename[128];
	DWORD data[4];
	int fd;

	sprintf(cpuid_filename, "/dev/cpu/0/cpuid");

	fd = open(cpuid_filename, O_RDONLY);

	if ( fd < 0 )
	{
		if ( errno == ENXIO )
		{
			fprintf(stderr, "cpuid: No CPUID on processor 0\n");
			return false;
		}
		else if (errno == EIO )
		{
			fprintf(stderr, "cpuid: CPU 0 doesn't support CPUID\n");
			return false;
		}
		else
		{
			perror("cpuid:open");
			return false;
		}
	}
  
	if ( pread(fd, &data, sizeof data, index) != sizeof data )
	{
		perror("cpuid:pread");
		return false;
	}

	*eax=data[0];
	*ebx=data[1];
	*ecx=data[2];
	*edx=data[3];

	close(fd);

	return true;
}

BOOL ReadPciConfigDwordEx(DWORD pciAddress, DWORD regAddress, PDWORD value)
{
	char pcidev_filename[128];
	int fd;
	DWORD data;
	DWORD bus, device, function;

	bus=(pciAddress >> 8) & 0xff;
	device=(pciAddress >> 3) & 0x1f;
	function=pciAddress & 0x7;

	sprintf(pcidev_filename, "/proc/bus/pci/%02x/%02x.%x",bus,device,function);

	fd = open(pcidev_filename, O_RDONLY);

	if ( fd < 0 )
	{
		if ( errno == ENXIO )
		{
			fprintf(stderr, "ReadPciConfigDwordEx: ENXIO error\n");
			return false;
		}
		else if (errno == EIO )
		{
			fprintf(stderr, "ReadPciConfigDwordEx: EIO error\n");
			return false;
		}
		else
		{
			perror("ReadPciConfigDwordEx: open");
			return false;
		}
	}
  
	if ( pread(fd, &data, sizeof data, regAddress) != sizeof data )
	{
		perror("ReadPciConfigDwordEx: pread");
		return false;
	}
	
	*value = data;
	
	close(fd);
	
	return true;
}

BOOL WritePciConfigDwordEx(DWORD pciAddress, DWORD regAddress, DWORD value)
{
	char pcidev_filename[128];
	int fd;
	DWORD data;
	DWORD bus, device, function;
	
	bus=(pciAddress >> 8) & 0xff;
	device=(pciAddress >> 3) & 0x1f;
	function=pciAddress & 0x7;
	
	sprintf(pcidev_filename, "/proc/bus/pci/%02x/%02x.%x",bus,device,function);
	
	fd = open(pcidev_filename, O_WRONLY);
	
	if ( fd < 0 )
	{
		if ( errno == ENXIO )
		{
			fprintf(stderr, "WritePciConfigDwordEx: ENXIO error\n");
			return false;
		}
		else if (errno == EIO )
		{
			fprintf(stderr, "WritePciConfigDwordEx: EIO error\n");
			return false;
		}
		else
		{
			perror("WritePciConfigDwordEx: open");
			return false;
		}
	}
	
	data=value;
	
	if ( pwrite(fd, &data, sizeof data, regAddress) != sizeof data )
	{
		perror("WritePciConfigDwordEx: pwrite");
		return false;
	}
	
	close(fd);
	
	return true;
}

BOOL RdmsrPx(DWORD index, PDWORD eax, PDWORD edx, DWORD_PTR processAffinityMask)
{
	char msr_filename[128];
	DWORD data[2];
	int fd;
	DWORD processor=0;
	bool isValidProcessor;

	while (processor<MAX_CORES)
	{
		isValidProcessor=processAffinityMask & ((DWORD_PTR)1<<processor);

		if (isValidProcessor)
		{			
			sprintf(msr_filename, "/dev/cpu/%d/msr",processor);

			fd = open(msr_filename, O_RDONLY);

			if ( fd < 0 ) {
				if ( errno == ENXIO ) {
					fprintf(stderr, "RdmsrPx: Invalid %u processor\n",processor);
					return false;
				} else if (errno == EIO ) {
					fprintf(stderr, "RdmsrPx: CPU %u doesn't support MSR\n",processor);
					return false;
				} else {
					perror("RdmsrPx: open");
					return false;
				}
			}
	
  	
			if ( pread(fd, &data, sizeof data, index) != sizeof data )
			{
				perror("rdmsr: pread");
				return false;
			}
	
			*eax=data[0];
			*edx=data[1];
	
			close(fd);

			//This is intended because this procedure can't report more than 
			//one CPU MSR, so we will report the first processor in the mask
			//discarding the others.
			//In the write flavour of this procedure, it is useful to try
			//for all processors in the mask.
			return true;
		}
		processor++;
	}
	return true;
}

BOOL Rdmsr(DWORD index, PDWORD eax, PDWORD edx)
{
	return RdmsrPx(index, eax, edx, 0x1);
}

BOOL WrmsrPx(DWORD index, DWORD eax, DWORD edx, DWORD_PTR processAffinityMask)
{
	char msr_filename[128];
	DWORD data[2];
	int fd;
	DWORD processor=0;
	bool isValidProcessor;

// 	printf ("Mask: %x\n", processAffinityMask);

	while (processor<MAX_CORES) {

		isValidProcessor=(processAffinityMask>>processor) & 1;

		if (isValidProcessor) {

// 			printf ("processor %d is valid\n", processor);

			data[0]=eax;
			data[1]=edx;
	
			sprintf(msr_filename, "/dev/cpu/%u/msr",processor);
	
			fd = open(msr_filename, O_WRONLY);
	
			if ( fd < 0 )
			{
				if ( errno == ENXIO )
				{
					fprintf(stderr, "WrmsrPx: Invalid %u processor\n",processor);
					return false;
				}
				else if (errno == EIO )
				{
					fprintf(stderr, "WrmsrPx: CPU %u doesn't support MSR\n",processor);
					return false;
				}
				else
				{
					fprintf(stderr, "WrmsrPx: open");
					return false;
				}
			}
  	
			if ( pwrite(fd, &data, sizeof data, index) != sizeof data)
			{
				if (errno == EIO) 
					fprintf(stderr, "wrmsr: CPU %d cannot set MSR %X to %X %X\n", processor, index, data[0], data[1]);
				else
					fprintf(stderr, "WrmsrPx pread Errno %x\n",errno);
				close(fd);
				return false;
			}

			close(fd);
			
			/*
			 * Removes the current processor from the mask to optimize the write loop with
			 * a further check on processAffinityMask variable
			 */
			processAffinityMask ^= (DWORD_PTR)1 << processor;
	
		}

		//Breaks the loop if processAffinityMask is empty.
		if (processAffinityMask==0) {
			return true;
		}

		processor++;

	}

	return true;
}

BOOL Wrmsr(DWORD index, DWORD eax, DWORD edx)
{
	return WrmsrPx(index, eax, edx, 0x1);
}

void Sleep (DWORD ms) {
	usleep (ms*1000);
	return;
}

int GetTickCount ()
{
#ifndef CLOCK_MONOTONIC_HR
#define CLOCK_MONOTONIC_HR CLOCK_MONOTONIC
#endif
	struct timespec tp;

	if (clock_gettime(CLOCK_MONOTONIC_HR, &tp))
		return -1;

	tp.tv_sec *= 1000;
	tp.tv_nsec /= 1000000;
	tp.tv_sec += tp.tv_nsec;
	return tp.tv_sec;
}	
