/*
 * K10PerformanceCounters.cpp
 *
 * Collection of static method to exploit performance counters. Since many architectures shares the same
 * performance counter structure, counting and registers, we use this technique to not repeat a lot of times
 * the same common code
 *
 *  Created on: 31/lug/2011
 *      Author: paolo
 */

#include "Processor.h"
#include "PerformanceCounter.h"
#include "PCIRegObject.h"
#include "MSRObject.h"
#include "Signal.h"

void Processor::K10PerformanceCounters::perfMonitorCPUUsage(class Processor *p)
{
	PerformanceCounter *perfCounter;
	MSRObject *tscCounter; //We need the timestamp counter too to determine the cpu usage in percentage

	DWORD cpuIndex, nodeId, coreId;
	PROCESSORMASK cpuMask;
	unsigned int perfCounterSlot;

	uint64_t usage;

	// These two pointers will refer to two arrays containing previous performance counter values
	// and previous Time Stamp counters. We need these to obtain instantaneous CPU usage information
	uint64_t *prevPerfCounters;
	uint64_t *prevTSCCounters;

	try
	{
		p->setNode(p->ALL_NODES);
		p->setCore(p->ALL_CORES);

		cpuMask = p->getMask();
		/* We do this to do some "caching" of the mask, instead of calculating each time
		 we need to retrieve the time stamp counter */

		// Allocating space for previous values of counters.
		prevPerfCounters = (uint64_t *) calloc(p->getProcessorCores() * p->getProcessorNodes(), sizeof(uint64_t));
		prevTSCCounters = (uint64_t *) calloc(p->getProcessorCores() * p->getProcessorNodes(), sizeof(uint64_t));

		// MSR Object to retrieve the time stamp counter for all the nodes and all the processors
		tscCounter = new MSRObject();

		//Creates a new performance counter, for now we set slot 0, but we will
		//use the findAvailable slot method to find an available method to be used
		perfCounter = new PerformanceCounter(cpuMask, 0, p->getMaxSlots());

		//Event 0x76 is Idle Counter
		perfCounter->setEventSelect(0x76);
		perfCounter->setCountOsMode(true);
		perfCounter->setCountUserMode(true);
		perfCounter->setCounterMask(0);
		perfCounter->setEdgeDetect(false);
		perfCounter->setEnableAPICInterrupt(false);
		perfCounter->setInvertCntMask(false);
		perfCounter->setUnitMask(0);
		perfCounter->setMaxSlots(p->getMaxSlots());

		//Finds an available slot for our purpose
		perfCounterSlot = perfCounter->findAvailableSlot();

		//findAvailableSlot() returns -2 in case of error
		if (perfCounterSlot == 0xfffffffe)
			throw "unable to access performance counter slots";

		//findAvailableSlot() returns -1 in case there aren't available slots
		if (perfCounterSlot == 0xffffffff)
			throw "unable to find an available performance counter slot";

		printf("Performance counter will use slot #%d\n", perfCounterSlot);

		//In case there are no errors, we program the object with the slot itself has found
		perfCounter->setSlot(perfCounterSlot);

		// Program the counter slot
		if (!perfCounter->program())
			throw "unable to program performance counter parameters";

		// Enable the counter slot
		if (!perfCounter->enable())
			throw "unable to enable performance counters";

		/* Here we take a snapshot of the performance counter and a snapshot of the time
		 * stamp counter to initialize the arrays to let them not show erratic huge numbers
		 * on first step
		 */

		if (!perfCounter->takeSnapshot())
		{
			throw "unable to retrieve performance counter data";
			return;
		}

		if (!tscCounter->readMSR(TIME_STAMP_COUNTER_REG, cpuMask))
		{
			throw "unable to retrieve time stamp counter";
			return;
		}

		cpuIndex = 0;
		for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
		{
			for (coreId = 0; coreId < p->getProcessorCores(); coreId++)
			{
				prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);
				prevTSCCounters[cpuIndex] = tscCounter->getBits(cpuIndex, 0, 64);
				cpuIndex++;
			}
		}

		Signal::activateSignalHandler(SIGINT);
		printf("Values >100%% can be expected if the CPU is in a Boosted State\n");

		while (!Signal::getSignalStatus())
		{
			if (!perfCounter->takeSnapshot())
			{
				throw "unable to retrieve performance counter data";
				return;
			}

			if (!tscCounter->readMSR(TIME_STAMP_COUNTER_REG, cpuMask))
			{
				throw "unable to retrieve time stamp counter";
				return;
			}

			cpuIndex = 0;

			for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
			{
				printf("\nNode %d -", nodeId);

				for (coreId = 0x0; coreId < p->getProcessorCores(); coreId++)
				{
 					usage = ((perfCounter->getCounter(cpuIndex)) - prevPerfCounters[cpuIndex]) * 100;
 					usage /= tscCounter->getBits(cpuIndex, 0, 64) - prevTSCCounters[cpuIndex];
 
 					printf(" c%d:%d%%", coreId, (unsigned int) usage);
 
 					prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);
 					prevTSCCounters[cpuIndex] = tscCounter->getBits(cpuIndex, 0, 64);

					cpuIndex++;
				}
			}
			Sleep(1000);
		}

		perfCounter->disable();

		printf ("CTRL-C executed. Cleaning on exit...\n");

	} catch (char const *str) {

		if (perfCounter->getEnabled()) perfCounter->disable();

		printf("K10PerformanceCounters.cpp::perfMonitorCPUUsage - %s\n", str);

	}

	free(perfCounter);
	free(tscCounter);
	free(prevPerfCounters);
	free(prevTSCCounters);

	return;

}

void Processor::K10PerformanceCounters::perfMonitorFPUUsage(class Processor *p)
{
	PerformanceCounter *perfCounter;
	MSRObject *tscCounter; //We need the timestamp counter too to determine the cpu usage in percentage

	DWORD cpuIndex, nodeId, coreId;
	PROCESSORMASK cpuMask;
	unsigned int perfCounterSlot;

	uint64_t usage;

	// These two pointers will refer to two arrays containing previous performance counter values
	// and previous Time Stamp counters. We need these to obtain instantaneous CPU usage information
	uint64_t *prevPerfCounters;
	uint64_t *prevTSCCounters;

	try {

		p->setNode(p->ALL_NODES);
		p->setCore(p->ALL_CORES);

		cpuMask = p->getMask();
		/* We do this to do some "caching" of the mask, instead of calculating each time
		 we need to retrieve the time stamp counter */

		// Allocating space for previous values of counters.
		prevPerfCounters = (uint64_t *) calloc(p->getProcessorCores() * p->getProcessorNodes(), sizeof(uint64_t));
		prevTSCCounters = (uint64_t *) calloc(p->getProcessorCores() * p->getProcessorNodes(), sizeof(uint64_t));

		// MSR Object to retrieve the time stamp counter for all the nodes and all the processors
		tscCounter = new MSRObject();

		//Creates a new performance counter, for now we set slot 0, but we will
		//use the findAvailable slot method to find an available method to be used
		perfCounter = new PerformanceCounter(cpuMask, 0, p->getMaxSlots());

		//Event 0x76 is Idle Counter
		perfCounter->setEventSelect(0x1);
		perfCounter->setCountOsMode(true);
		perfCounter->setCountUserMode(true);
		perfCounter->setCounterMask(0);
		perfCounter->setEdgeDetect(false);
		perfCounter->setEnableAPICInterrupt(false);
		perfCounter->setInvertCntMask(false);
		perfCounter->setUnitMask(0);

		//Finds an available slot for our purpose
		perfCounterSlot = perfCounter->findAvailableSlot();

		//findAvailableSlot() returns -2 in case of error
		if (perfCounterSlot == 0xfffffffe)
			throw "unable to access performance counter slots";

		//findAvailableSlot() returns -1 in case there aren't available slots
		if (perfCounterSlot == 0xffffffff)
			throw "unable to find an available performance counter slot";

		printf("Performance counter will use slot #%d\n", perfCounterSlot);

		//In case there are no errors, we program the object with the slot itself has found
		perfCounter->setSlot(perfCounterSlot);

		// Program the counter slot
		if (!perfCounter->program())
			throw "unable to program performance counter parameters";

		// Enable the counter slot
		if (!perfCounter->enable())
			throw "unable to enable performance counters";

		/* Here we take a snapshot of the performance counter and a snapshot of the time
		 * stamp counter to initialize the arrays to let them not show erratic huge numbers
		 * on first step
		 */

		if (!perfCounter->takeSnapshot())
			throw "unable to retrieve performance counter data";

		if (!tscCounter->readMSR(TIME_STAMP_COUNTER_REG, cpuMask))
			throw "unable to retrieve time stamp counter";

		cpuIndex = 0;
		for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
		{
			for (coreId = 0x0; coreId < p->getProcessorCores(); coreId++)
			{
				prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);
				prevTSCCounters[cpuIndex] = tscCounter->getBits(cpuIndex, 0, 64);
				cpuIndex++;
			}
		}

		Signal::activateSignalHandler(SIGINT);

		while (!Signal::getSignalStatus())
		{
			if (!perfCounter->takeSnapshot())
				throw "unable to retrieve performance counter data";

			if (!tscCounter->readMSR(TIME_STAMP_COUNTER_REG, cpuMask))
				throw "unable to retrieve time stamp counter";

			cpuIndex = 0;

			for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
			{
				printf("Node %d -", nodeId);

				for (coreId = 0x0; coreId < p->getProcessorCores(); coreId++)
				{

					usage = ((perfCounter->getCounter(cpuIndex)) - prevPerfCounters[cpuIndex]) * 100;
					usage /= tscCounter->getBits(cpuIndex, 0, 64) - prevTSCCounters[cpuIndex];

					printf(" c%u:%u%%", coreId, (unsigned int) usage);

					prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);
					prevTSCCounters[cpuIndex] = tscCounter->getBits(cpuIndex, 0, 64);

					cpuIndex++;
				}
				printf("\n");
			}
			Sleep(1000);
		}

		perfCounter->disable();

		printf ("CTRL-C executed. Cleaning on exit...\n");

	} catch (char const *str) {

		if (perfCounter->getEnabled()) perfCounter->disable();

		printf("K10PerformanceCounters.cpp::perfMonitorCPUUsage - %s\n", str);
	}

	free(perfCounter);
	free(tscCounter);
	free(prevPerfCounters);
	free(prevTSCCounters);

	return;

}

void Processor::K10PerformanceCounters::perfMonitorDCMA(class Processor *p)
{
	PerformanceCounter *perfCounter;

	DWORD cpuIndex, nodeId, coreId;
	PROCESSORMASK cpuMask;
	unsigned int perfCounterSlot;

	uint64_t misses;

	// This pointers will refer an array containing previous performance counter values
	uint64_t *prevPerfCounters;

	try {

		p->setNode(p->ALL_NODES);
		p->setCore(p->ALL_CORES);

		cpuMask = p->getMask();
		/* We do this to do some "caching" of the mask, instead of calculating each time
		 we need to retrieve the time stamp counter */

		// Allocating space for previous values of counters.
		prevPerfCounters = (uint64_t *) calloc(
				p->getProcessorCores() * p->getProcessorNodes(),
				sizeof(uint64_t));

		//Creates a new performance counter, for now we set slot 0, but we will
		//use the findAvailable slot method to find an available method to be used
		perfCounter = new PerformanceCounter(cpuMask, 0, p->getMaxSlots());

		//Event 0x76 is Idle Counter
		perfCounter->setEventSelect(0x47);
		perfCounter->setCountOsMode(true);
		perfCounter->setCountUserMode(true);
		perfCounter->setCounterMask(0);
		perfCounter->setEdgeDetect(false);
		perfCounter->setEnableAPICInterrupt(false);
		perfCounter->setInvertCntMask(false);
		perfCounter->setUnitMask(0);

		//Finds an available slot for our purpose
		perfCounterSlot = perfCounter->findAvailableSlot();

		//findAvailableSlot() returns -2 in case of error
		if (perfCounterSlot == 0xfffffffe)
			throw "unable to access performance counter slots";

		//findAvailableSlot() returns -1 in case there aren't available slots
		if (perfCounterSlot == 0xffffffff)
			throw "unable to find an available performance counter slot";

		printf("Performance counter will use slot #%d\n", perfCounterSlot);

		//In case there are no errors, we program the object with the slot itself has found
		perfCounter->setSlot(perfCounterSlot);

		// Program the counter slot
		if (!perfCounter->program())
			throw "unable to program performance counter parameters";

		// Enable the counter slot
		if (!perfCounter->enable())
			throw "unable to enable performance counters";

		/* Here we take a snapshot of the performance counter and a snapshot of the time
		 * stamp counter to initialize the arrays to let them not show erratic huge numbers
		 * on first step
		 */

		if (!perfCounter->takeSnapshot())
			throw "unable to retrieve performance counter data";

		cpuIndex = 0;
		for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
		{
			for (coreId = 0x0; coreId < p->getProcessorCores(); coreId++)
			{
				prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);
				cpuIndex++;
			}
		}

		Signal::activateSignalHandler(SIGINT);

		while (!Signal::getSignalStatus())
		{
			if (!perfCounter->takeSnapshot())
				throw "unable to retrieve performance counter data";

			cpuIndex = 0;

			for (nodeId = 0; nodeId < p->getProcessorNodes(); nodeId++)
			{
				printf("Node %d -", nodeId);

				for (coreId = 0x0; coreId < p->getProcessorCores(); coreId++)
				{
					misses = perfCounter->getCounter(cpuIndex) - prevPerfCounters[cpuIndex];

					printf(" c%u:%0.3fk", coreId, (float) (misses/1000.0f));

					prevPerfCounters[cpuIndex] = perfCounter->getCounter(cpuIndex);

					cpuIndex++;
				}
				printf("\n");
			}
			Sleep(1000);
		}

		perfCounter->disable();

		printf ("CTRL-C executed. Cleaning on exit...\n");

	} catch (char const *str) {

		if (perfCounter->getEnabled()) perfCounter->disable();

		printf("K10PerformanceCounters.cpp::perfMonitorCPUUsage - %s\n", str);

	}

	free(perfCounter);
	free(prevPerfCounters);

	return;

}



void Processor::K10PerformanceCounters::perfCounterGetInfo (class Processor *p) {

	PerformanceCounter *performanceCounter;
	DWORD node, core, slot;

	printf ("Caption:\n");
	printf ("Evt:\tperformance counter event\n");
	printf ("En:\tperformance counter is enabled\n");
	printf ("U:\tperformance counter will count usermode instructions\n");
	printf ("OS:\tperformance counter will counter Os/kernel instructions\n");
	printf ("cMsk:\tperformance counter mask (see processor manual reference)\n");
	printf ("ED:\tcounting on edge detect, else counting on level detect\n");
	printf ("APIC:\tif set, an APIC interrupt will be issued on counter overflow\n");
	printf ("icMsk:\tif set, mask is inversed (see processor manual reference)\n");
	printf ("uMsk:\tunit mask (see processor manual reference)\n\n");

	for (node = 0; node < p->getProcessorNodes(); node++)
	{
		printf ("--- Node %d\n", node);

		p->setNode(node);
		p->setCore(ALL_CORES);

		for (slot = 0; slot < p->getMaxSlots(); slot++)
		{
			performanceCounter = new PerformanceCounter(p->getMask(), slot, p->getMaxSlots());

			for (core = 0; core < p->getProcessorCores(); core++)
			{
				if (!performanceCounter->fetch (core))
				{
					printf ("K10PerformanceCounters.cpp::perfCounterGetInfo - unable to read performance counter register\n");
					free (performanceCounter);
					return;
				}

				printf ("Slot %d core %d - evt:0x%x En:%d U:%d OS:%d cMsk:%x ED:%d APIC:%d icMsk:%x uMsk:%x\n",
						slot,
						core,
						performanceCounter->getEventSelect(),
						performanceCounter->getEnabled(),
						performanceCounter->getCountUserMode(),
						performanceCounter->getCountOsMode(),
						performanceCounter->getCounterMask(),
						performanceCounter->getEdgeDetect(),
						performanceCounter->getEnableAPICInterrupt(),
						performanceCounter->getInvertCntMask(),
						performanceCounter->getUnitMask()
						);
			}
			free (performanceCounter);
		}
	}
}
