#include <stdio.h>
#include <stdlib.h>
#include "Processor.h"

#define POLICY_ROCKET 0
#define POLICY_STEP 1
#define POLICY_DYNAMIC 2

#define DEFAULT_SAMPLING_RATE 1000 //Default sampling rate in milliseconds

class Scaler {
private:
	int samplingRate;
	
	int upPolicy;
	int downPolicy;
	
	int upperThreshold;
	int lowerThreshold;
	
	int midUpperThreshold;
	int midLowerThreshold;
	
	int fastestPState;
	int slowestPState;
	Processor *processor;
	
	int fullFrequency;
	
public:
	void setSamplingFrequency (int);
	
	void setUpPolicy (int);
	void setDownPolicy (int);
	
	void setUpperThreshold (int);
	void setLowerThreshold (int);
	
	Scaler (class Processor *);
	void beginScaling ();
};
