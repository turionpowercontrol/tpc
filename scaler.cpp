#include "scaler.h"

//TODO: IMPORTANT ********* Scaler must be completely revised

Scaler::Scaler(class Processor *prc) {

	processor = prc;

	processor->setNode(0);
	processor->setCore(0);

	samplingRate = DEFAULT_SAMPLING_RATE;

	policy = POLICY_STEP;

	upperThreshold = 70;
	lowerThreshold = 20;

	midUpperThreshold = (100 + upperThreshold) >> 1;
	midLowerThreshold = (100 - lowerThreshold) >> 1;

	PState ps(0);
	ps = processor->getMaximumPState();

	slowestPowerState = ps.getPState();

}

void Scaler::setSamplingFrequency(int msSmpFreq) {
	samplingRate = msSmpFreq;
}

void Scaler::setPolicy(int policy) {
	this->policy = policy;
}

void Scaler::setUpperThreshold(int thres) {

	if (thres > 100)
		upperThreshold = 100;
	else
		upperThreshold = thres;

	midUpperThreshold = (100 + upperThreshold) >> 1;
}

void Scaler::setLowerThreshold(int thres) {

	if (thres < 0)
		lowerThreshold = 0;
	else
		lowerThreshold = thres;

	midLowerThreshold = (100 - lowerThreshold) >> 1;
}

/* Scaling methods:

 STEP	- Stepped scaling, means that when CPU usage goes over 70% for a core,
 the scaler brings the core at the immediate faster pstate.
 When processor usage goes below 20% for a core, the scaler brings the core
 at the immediate slower pstate.

 ROCKET	- If CPU core usage goes above 70%, core is set to full frequency. If
 cpu core usage goes below 20%, core is immediately set to slower pstate.

 DYNAMIC - If CPU core usage is 100%, core is set to full frequency. If CPU
 core usage is above 85%, core is set a faster pstate to two step ahead.
 If CPU core usage is above 70%, core is set to the immediate next faster
 pstate.

 If cpu core usage is 0%, core is set to slowest frequency. If CPU core
 usage is below 10%, core is set to a slower pstate two step backward.
 If CPU usage is below 20%, core is set to one step backward.
 */

int Scaler::initializeCounters() {
	DWORD cpuIndex, nodeId, coreId;
	PROCESSORMASK cpuMask;
	unsigned int perfCounterSlot;

	try {

		this->processor->setNode(this->processor->ALL_NODES);
		this->processor->setCore(this->processor->ALL_CORES);

		cpuMask = this->processor->getMask();
		/* We do this to do some "caching" of the mask, instead of calculating each time
		 we need to retrieve the time stamp counter */

		// Allocating space for previous values of counters.
		this->prevPerfCounters = (uint64_t *) calloc(
				this->processor->getProcessorCores() * this->processor->getProcessorNodes(),
				sizeof(uint64_t));
		this->prevTSCCounters = (uint64_t *) calloc(
				this->processor->getProcessorCores() * this->processor->getProcessorNodes(),
				sizeof(uint64_t));

		// MSR Object to retrieve the time stamp counter for all the nodes and all the processors
		this->tscCounter = new MSRObject();

		//Creates a new performance counter, for now we set slot 0, but we will
		//use the findAvailable slot method to find an available method to be used
		this->perfCounter = new PerformanceCounter(cpuMask, 0, this->processor->getMaxSlots());

		//Event 0x76 is Idle Counter
		perfCounter->setEventSelect(0x76);
		perfCounter->setCountOsMode(true);
		perfCounter->setCountUserMode(true);
		perfCounter->setCounterMask(0);
		perfCounter->setEdgeDetect(false);
		perfCounter->setEnableAPICInterrupt(false);
		perfCounter->setInvertCntMask(false);
		perfCounter->setUnitMask(0);

		//Finds an available slot for our purpose
		perfCounterSlot = this->perfCounter->findAvailableSlot();

		//findAvailableSlot() returns -2 in case of error
		if (perfCounterSlot == 0xfffffffe)
			throw "unable to access performance counter slots";

		//findAvailableSlot() returns -1 in case there aren't available slots
		if (perfCounterSlot == 0xffffffff)
			throw "unable to find an available performance counter slot";

		printf("Performance counter will use slot #%d\n", perfCounterSlot);

		//In case there are no errors, we program the object with the slot itself has found
		this->perfCounter->setSlot(perfCounterSlot);

		// Program the counter slot
		if (!this->perfCounter->program())
			throw "unable to program performance counter parameters";

		// Enable the counter slot
		if (!this->perfCounter->enable())
			throw "unable to enable performance counters";

		/* Here we take a snapshot of the performance counter and a snapshot of the time
		 * stamp counter to initialize the arrays to let them not show erratic huge numbers
		 * on first step
		 */

		if (!this->perfCounter->takeSnapshot())
			throw "unable to retrieve performance counter data";

		if (!this->tscCounter->readMSR(TIME_STAMP_COUNTER_REG, cpuMask))
			throw "unable to retrieve time stamp counter";

		cpuIndex = 0;
		for (nodeId = 0; nodeId < this->processor->getProcessorNodes(); nodeId++) {
			for (coreId = 0x0; coreId < this->processor->getProcessorCores(); coreId++) {
				this->prevPerfCounters[cpuIndex] = this->perfCounter->getCounter(cpuIndex);
				this->prevTSCCounters[cpuIndex]
						= this->tscCounter->getBits(cpuIndex, 0, 64);
				cpuIndex++;
			}
		}

		this->perfCounter->disable();

	} catch (char const *str) {

		if (this->perfCounter->getEnabled())
			this->perfCounter->disable();

		free(this->perfCounter);
		free(this->tscCounter);
		free(this->prevPerfCounters);
		free(this->prevTSCCounters);

		printf("Scaler.cpp::initializeCounters - %s\n", str);

		return -1; //In case of error, we return -1

	}

	return 0; //In case of success, we return 0

}

void Scaler::loopPolicyRocket() {

	unsigned char reqPState;
	unsigned int enabledPowerStates;
	DWORD units, cpuIndex, targetUnit, nodeIndex, coreIndex;
	uint64_t deltaUsage;
	unsigned int divisor;

	PState **ps;

	units=this->processor->getProcessorCores()*this->processor->getProcessorNodes();

	ps=(PState **)calloc (units, sizeof (PState *));

	for (cpuIndex=0;cpuIndex<units;cpuIndex++)
		ps[cpuIndex]=new PState(2);

	divisor=(1000000/1000)*this->samplingRate;

	enabledPowerStates=this->processor->getMaximumPState().getPState();

	Signal::activateSignalHandler( SIGINT);

	while (!Signal::getSignalStatus()) {

		if (!this->perfCounter->takeSnapshot())
			throw "unable to retrieve performance counter data";

		cpuIndex=0;

		for (nodeIndex=0;nodeIndex<this->processor->getProcessorNodes();nodeIndex++) {

			this->processor->setNode(nodeIndex);

			for (coreIndex=0;coreIndex<this->processor->getProcessorCores();coreIndex++) {

				this->processor->setCore(coreIndex);

				reqPState=ps[cpuIndex]->getPState();

				deltaUsage = ((this->perfCounter->getCounter(cpuIndex))
					- this->prevPerfCounters[cpuIndex])/divisor;

				targetUnit=(reqPState*enabledPowerStates)+cpuIndex;

				/*printf ("CPU %d Usage: %d Current pstate: %d - Raise Freq: %d Reduce Freq: %d \n ",
						cpuIndex, deltaUsage, reqPState, raiseTable[targetUnit], reduceTable[targetUnit]);*/

				if (deltaUsage>raiseTable[targetUnit]){ reqPState=0; }
				else if (deltaUsage<reduceTable[targetUnit]){ reqPState++; }

				ps[cpuIndex]->setPState(reqPState);

				this->processor->forcePState(ps[cpuIndex]->getPState());

				this->prevPerfCounters[cpuIndex] = this->perfCounter->getCounter(cpuIndex);

				cpuIndex++;

			}

		}

		//printf("\n");

		Sleep(this->samplingRate);

	}

	for (cpuIndex=0;cpuIndex<units;cpuIndex++)
		free (ps[cpuIndex]);

	free (ps);
}

void Scaler::loopPolicyStep() {

	unsigned char reqPState;
	unsigned int enabledPowerStates;
	DWORD units, cpuIndex, targetUnit, nodeIndex, coreIndex;
	uint64_t deltaUsage;
	unsigned int divisor;

	PState **ps;

	units=this->processor->getProcessorCores()*this->processor->getProcessorNodes();

	ps=(PState **)calloc (units, sizeof (PState *));

	for (cpuIndex=0;cpuIndex<units;cpuIndex++)
		ps[cpuIndex]=new PState(2);

	divisor=(1000000/1000)*this->samplingRate;

	enabledPowerStates=this->processor->getMaximumPState().getPState();

	Signal::activateSignalHandler( SIGINT);

	while (!Signal::getSignalStatus()) {

		if (!this->perfCounter->takeSnapshot())
			throw "unable to retrieve performance counter data";

		cpuIndex=0;

		for (nodeIndex=0;nodeIndex<this->processor->getProcessorNodes();nodeIndex++) {

			this->processor->setNode(nodeIndex);

			for (coreIndex=0;coreIndex<this->processor->getProcessorCores();coreIndex++) {

				this->processor->setCore(coreIndex);

				reqPState=ps[cpuIndex]->getPState();

				deltaUsage = ((this->perfCounter->getCounter(cpuIndex))
					- this->prevPerfCounters[cpuIndex])/divisor;

				targetUnit=(reqPState*enabledPowerStates)+cpuIndex;

				/*printf ("CPU %d Usage: %d Current pstate: %d - Raise Freq: %d Reduce Freq: %d \n ",
						cpuIndex, deltaUsage, reqPState, raiseTable[targetUnit], reduceTable[targetUnit]);*/

				if (deltaUsage>raiseTable[targetUnit]){ if (reqPState!=0) reqPState--; }
				else if (deltaUsage<reduceTable[targetUnit]){ reqPState++; }

				ps[cpuIndex]->setPState(reqPState);

				this->processor->forcePState(ps[cpuIndex]->getPState());

				this->prevPerfCounters[cpuIndex] = this->perfCounter->getCounter(cpuIndex);

				cpuIndex++;

			}

		}

		//printf("\n");

		Sleep(this->samplingRate);

	}

	for (cpuIndex=0;cpuIndex<units;cpuIndex++)
		free (ps[cpuIndex]);

	free (ps);
}


void Scaler::createPerformanceTables () {

	PState ps(0);
	PState ps_back(0);
	unsigned int i,unitIndex;
	unsigned int nodeIndex, coreIndex;
	unsigned int units; //units are total number of processors in the system
	unsigned int targetUnit;
	unsigned int enabledPowerStates;
	unsigned int diff_frequency;

	units=this->processor->getProcessorCores()*this->processor->getProcessorNodes();
	enabledPowerStates=this->processor->getMaximumPState().getPState();

	raiseTable=(uint64_t *)calloc (units*(enabledPowerStates+1), sizeof(uint64_t));
	reduceTable=(uint64_t *)calloc (units*(enabledPowerStates+1), sizeof(uint64_t));

	for (i=0;i<=enabledPowerStates;i++) {

		printf ("Power State %d:" , i);

		ps.setPState(i);
		unitIndex=0;

		for (nodeIndex=0;nodeIndex<this->processor->getProcessorNodes();nodeIndex++) {

			this->processor->setNode(nodeIndex);

			for (coreIndex=0;coreIndex<this->processor->getProcessorCores();coreIndex++) {

				this->processor->setCore (coreIndex);

				targetUnit=(i*enabledPowerStates) + unitIndex;

				if (i==this->processor->getMaximumPState().getPState()) {
					reduceTable[targetUnit]=0;
					raiseTable[targetUnit]=this->processor->getFrequency(ps)*this->upperThreshold/100;
				}
				else
				{
					ps_back.setPState(i+1);
					diff_frequency=this->processor->getFrequency(ps)-this->processor->getFrequency(ps_back);

					raiseTable[targetUnit]=(diff_frequency*this->upperThreshold/100)+this->processor->getFrequency(ps_back);
					reduceTable[targetUnit]=(diff_frequency*this->lowerThreshold/100)+this->processor->getFrequency(ps_back);

				}

				//printf ("Ra:%ld Re:%ld ",raiseTable[targetUnit], reduceTable[targetUnit]);

				unitIndex++;

			}
		}

	printf ("\n");

	}

	return;

}

void Scaler::beginScaling() {

	if (initializeCounters()) {
		perror(
				"Scaler::beginScaling - performance counters initialization failed\n");
		return;
	}

	createPerformanceTables();

	this->perfCounter->enable();

	switch (this->policy) {
	case POLICY_STEP:
		loopPolicyStep(); //loop will be terminated with a CTRL-C command
		break;
	case POLICY_ROCKET:
		loopPolicyRocket();
		break;
	}

	printf ("CTRL-C pressed. Terminating scaler and freeing resources... ");

	this->perfCounter->disable();

	if (this->perfCounter->getEnabled())
		this->perfCounter->disable();

	free(this->perfCounter);
	free(this->tscCounter);
	free(this->prevPerfCounters);
	free(this->prevTSCCounters);

	free(this->raiseTable);
	free(this->reduceTable);

	printf ("done.\n");

}
