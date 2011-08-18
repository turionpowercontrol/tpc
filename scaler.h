#include <stdio.h>
#include <stdlib.h>
#include "Processor.h"
#include "MSRObject.h"
#include "PerformanceCounter.h"
#include "Signal.h"

#define POLICY_ROCKET 0
#define POLICY_STEP 1

#define DEFAULT_SAMPLING_RATE 1000 //Default sampling rate in milliseconds

class Scaler {
private:
	int samplingRate;
	
	int policy;
	
	int upperThreshold;
	int lowerThreshold;

	int midUpperThreshold;
	int midLowerThreshold;

	Processor *processor;
	
	unsigned char slowestPowerState;

	PerformanceCounter *perfCounter;
	MSRObject *tscCounter;
	uint64_t *prevPerfCounters;
	uint64_t *prevTSCCounters;
	
	uint64_t *raiseTable;
	uint64_t *reduceTable;

	int initializeCounters ();
	void loopPolicyRocket ();
	void loopPolicyStep ();
	void createPerformanceTables ();

public:
	void setSamplingFrequency (int);
	
	void setPolicy (int);
	
	void setUpperThreshold (int);
	void setLowerThreshold (int);
	
	Scaler (class Processor *);
	void beginScaling ();
};
