class Interlagos: protected Processor
{
private:

	//Private methods for HT Link support
	DWORD getHTLinkSpeed(DWORD link, DWORD Sublink);
	DWORD getHTLinkWidth(DWORD link, DWORD Sublink, DWORD *WidthIn, DWORD *WidthOut, bool *pfCoherent, bool *pfUnganged);
	DWORD getHTLinkDistributionTarget(DWORD link, DWORD *DstLnk, DWORD *DstNode);

	void printRoute(DWORD);

	bool setDramController(DWORD device);
	int getDramFrequency(DWORD device, DWORD *T_mode);
	bool getDramValid(DWORD device);

	//DRAM timing registers
	void getDramTiming(DWORD device, /* 0 or 1 */ DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras, 
			      DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *Tcwl, DWORD *T_faw, DWORD *TrwtWB, 
			DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwrsdsc, DWORD *Trdrdsdsc, DWORD *Tref,
			DWORD *Trfc0, DWORD *Trfc1, DWORD *Trfc2, DWORD *Trfc3, DWORD *MaxRdLatency);

public:

	Interlagos();

 	static bool isProcessorSupported();

	void showFamilySpecs();
	void showHTC();
	void showHTLink();
	void showDramTimings ();

	float convertVIDtoVcore(DWORD);
	DWORD convertVcoretoVID(float);
	DWORD convertFDtoFreq(DWORD, DWORD);
	void convertFreqtoFD(DWORD, int *, int *);

	void setVID(PState, DWORD);
	void setFID(PState, float);
	void setDID(PState, float);

	DWORD getVID(PState);
	float getFID(PState);
	float getDID(PState);

	void setFrequency(PState , DWORD);
	void setVCore(PState, float);

	DWORD getFrequency(PState);
	float getVCore(PState);

	bool getPVIMode();

	void pStateEnable(PState) ;
	void pStateDisable(PState);
	bool pStateEnabled(PState);

	void setMaximumPState(PState);
	PState getMaximumPState();

	void forcePState(PState);

	void setNBVid(PState, DWORD);
	void setNBDid(PState, DWORD);
	DWORD getNBVid();

	DWORD getNBDid();
	DWORD getNBFid();
	DWORD getNBCOF();
	void setNBFid(DWORD);
	DWORD getMaxNBFrequency();

	DWORD minVID();
	DWORD maxVID();

	DWORD maxVIDByCore(DWORD core);
	DWORD minVIDByCore(DWORD core);

	DWORD startupPState();
	DWORD maxCPUFrequency();
	DWORD getNumBoostStates(void);
	void setNumBoostStates(DWORD);
	DWORD getBoost(void);
	void setBoost(bool);
	DWORD getTDP(void);

	//Virtual method to modify DRAM timings -- Needs testing, only for DDR3 at the moment
	DWORD setDramTiming(DWORD device, /* 0 or 1 */ DWORD Tcl, DWORD Trcd, DWORD Trp, DWORD Trtp, DWORD Tras, 
			    DWORD Trc, DWORD Twr, DWORD Trrd, DWORD Tcwl, DWORD T_mode);

	DWORD getTctlRegister(void);
	DWORD getTctlMaxDiff(void);

	DWORD getSlamTime(void);
	void setSlamTime(DWORD);

	/*DWORD getAltVidSlamTime (void);
	void setAltVidSlamTime (DWORD);*/

	DWORD getStepUpRampTime(void);
	DWORD getStepDownRampTime(void);
	void setStepUpRampTime(DWORD);
	void setStepDownRampTime(DWORD);

	//HTC Section - Read status
	bool HTCisCapable();
	bool HTCisEnabled();
	bool HTCisActive();
	bool HTChasBeenActive();
	DWORD HTCTempLimit();
	bool HTCSlewControl();
	DWORD HTCHystTemp();
	DWORD HTCPStateLimit();
	bool HTCLocked();

	DWORD getAltVID();

	//HTC Section - Change status
	void HTCEnable();

	void HTCDisable();
	void HTCsetTempLimit(DWORD);
	void HTCsetHystLimit(DWORD);
	void setAltVid(DWORD);
		
	//PSI_L bit
	bool getPsiEnabled();
	DWORD getPsiThreshold();
	void setPsiEnabled(bool);
	void setPsiThreshold(DWORD);

	//HyperTransport Section
	void setHTLinkSpeed(DWORD, DWORD);

	//Various settings
	bool getC1EStatus();
	void setC1EStatus(bool);

	// Autocheck mode
	void checkMode();

	//Performance counters
	void perfCounterGetInfo();
	void perfCounterGetValue(unsigned int);
	void perfMonitorCPUUsage();
	void perfMonitorFPUUsage();
	void perfMonitorDCMA();

	//Scaler helper methods
	void getCurrentStatus(struct procStatus *pStatus, DWORD core);
};