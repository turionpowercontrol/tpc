#include "scaler.h"

//TODO: IMPORTANT ********* Scaler must be completely revised

Scaler::Scaler (class Processor *prc) {

	processor=prc;
	samplingRate=DEFAULT_SAMPLING_RATE;
	
	upPolicy=POLICY_STEP;
	downPolicy=POLICY_STEP;
	
	upperThreshold=70;
	lowerThreshold=20;
	
	midUpperThreshold=(100+upperThreshold)>>1;
	midLowerThreshold=(100-lowerThreshold)>>1;
	
	fastestPState=0;
	
	PState ps(0);
	ps=processor->getMaximumPState();
	
	slowestPState=ps.getPState ();
	
	//Should be changed. We get maximum operating frequency from pstate 0 core 0. Works, but
	//it isn't the best solution.
	ps.setPState (0);
	processor->setCore(0);
	processor->setNode(0);
	fullFrequency=processor->getFrequency (ps);
}

void Scaler::setSamplingFrequency (int msSmpFreq) {
	samplingRate=msSmpFreq;
}

void Scaler::setUpPolicy (int policy) {
	upPolicy=policy;
}

void Scaler::setDownPolicy (int policy) {
	downPolicy=policy;
}

void Scaler::setUpperThreshold (int thres) {

	if (thres>100)
		upperThreshold=100;
	else
		upperThreshold=thres;
		
	midUpperThreshold=(100+upperThreshold)>>1;
}

void Scaler::setLowerThreshold (int thres) {

	if (thres<0)
		lowerThreshold=0;
	else
		lowerThreshold=thres;
		
	midLowerThreshold=(100-lowerThreshold)>>1;
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

void Scaler::beginScaling () {

	procStatus pStatus;
	DWORD *performanceRegisters;
	DWORD cpuUsage;
	int core;
	int dummy;
	PState ps(0);
	
	performanceRegisters=(DWORD *)calloc (processor->getProcessorCores(),sizeof (DWORD));

	processor->initUsageCounter (performanceRegisters);
	processor->setNode(processor->ALL_NODES);

	while (true) {
	
		for (core=0;core<processor->getProcessorCores();core++) {

			processor->setCore(core);

			cpuUsage=processor->getUsageCounter (performanceRegisters, core);
			processor->getCurrentStatus (&pStatus, core);
			cpuUsage=(fullFrequency*cpuUsage)/processor->convertFDtoFreq (pStatus.fid, pStatus.did);
			if ((cpuUsage>upperThreshold) && (pStatus.pstate!=fastestPState)) {
				if (upPolicy==POLICY_ROCKET) {
					ps.setPState (fastestPState);
					processor->forcePState (ps);
				} else if (upPolicy==POLICY_STEP) {
					ps.setPState (pStatus.pstate-1);
					processor->forcePState (ps);
				} else if (upPolicy==POLICY_DYNAMIC) {
					if (cpuUsage==100) {
						ps.setPState (fastestPState);
					} else if (cpuUsage>midUpperThreshold) {
						dummy=pStatus.pstate-2;
						if (dummy<=fastestPState)
							ps.setPState (fastestPState);
						else
							ps.setPState (dummy);
					} else {
						ps.setPState (pStatus.pstate-1);
					}
					processor->forcePState (ps);
				}
						
							
			}
			
			if ((cpuUsage<lowerThreshold) && (pStatus.pstate!=slowestPState)) {
				if (downPolicy==POLICY_ROCKET) {
					ps.setPState (slowestPState);
					processor->forcePState (ps);
				} else if (downPolicy==POLICY_STEP) {
					ps.setPState (pStatus.pstate+1);
					processor->forcePState (ps);
				} else if (downPolicy==POLICY_DYNAMIC) {
					if (cpuUsage==0) {
						ps.setPState (slowestPState);
					} else if (cpuUsage<midLowerThreshold) {
						if ((pStatus.pstate+2)>=slowestPState)
							ps.setPState (slowestPState);
						else
							ps.setPState (pStatus.pstate+2);
					} else {
						ps.setPState (pStatus.pstate+1);
					}
					processor->forcePState (ps);
				}
			}
				
			//printf ("core %d: %d ", core, cpuUsage);
		}
		
//		printf ("\n");
		
		Sleep (samplingRate);
	
	}
	
	free (performanceRegisters);
	
}
