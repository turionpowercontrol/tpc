class K10Processor: protected Processor {
private:

	//Private methods for HT Link support
	DWORD getHTLinkSpeed(DWORD link, DWORD Sublink);
	DWORD getHTLinkWidth(DWORD link, DWORD Sublink, DWORD *WidthIn,
			DWORD *WidthOut, bool *pfCoherent, bool *pfUnganged);
	DWORD getHTLinkDistributionTarget(DWORD link, DWORD *DstLnk,
			DWORD *DstNode);

	void printRoute(DWORD);

	int getDramFrequency (DWORD device);
	bool getDDR3Mode (DWORD device);
	bool getDramValid (DWORD device);


	//DRAM timing register Low
	void getDramTimingLow(
			DWORD device, // 0 or 1
			DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras,
			DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *Tcwl, DWORD *T_mode, DWORD *T_faw);

	//DRAM timing register High
	void getDramTimingHigh(DWORD device, DWORD *TrwtWB,
			DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwr,
			DWORD *Trdrd, DWORD *Tref, DWORD *Trfc0, DWORD *Trfc1,
			DWORD *Trfc2, DWORD *Trfc3, DWORD *MaxRdLatency);

public:

	K10Processor ();

 	static bool isProcessorSupported ();

	void showFamilySpecs ();
	void showHTC();
	void showHTLink();
	void showDramTimings ();

	float convertVIDtoVcore (DWORD);
	DWORD convertVcoretoVID (float);
	DWORD convertFDtoFreq (DWORD, DWORD);
	void convertFreqtoFD(DWORD, int *, int *);
		
	void setVID (PState , DWORD);
	void setFID (PState , DWORD);
	void setDID (PState , DWORD);

	DWORD getVID (PState);
	DWORD getFID (PState);
	DWORD getDID (PState);

	void setFrequency (PState , DWORD);
	void setVCore (PState, float);

	DWORD getFrequency (PState);
	float getVCore (PState);

	bool getPVIMode ();

	void pStateEnable (PState) ;
	void pStateDisable (PState);
	bool pStateEnabled (PState);

	void setMaximumPState (PState);
	PState getMaximumPState ();

	void forcePState (PState);

	void setNBVid (PState, DWORD);
	void setNBDid (PState, DWORD);
//	DWORD getNBVid ();
	DWORD getNBVid (PState);

	DWORD getNBDid (PState);
	DWORD getNBFid ();
	void setNBFid (DWORD);
	DWORD getMaxNBFrequency ();

	DWORD minVID ();
	DWORD maxVID ();

	DWORD maxVIDByCore( DWORD core );
	DWORD minVIDByCore( DWORD core );

	DWORD startupPState ();
	DWORD maxCPUFrequency ();

	//Virtual method to modify DRAM timings -- Needs testing, only for DDR3 at the moment
	DWORD setDramTiming(
			DWORD device, // 0 or 1
			DWORD Tcl, DWORD Trcd, DWORD Trp, DWORD Trtp, DWORD Tras,
			DWORD Trc, DWORD Twr, DWORD Trrd, DWORD Tcwl, DWORD T_mode);

	DWORD getTctlRegister (void);
	DWORD getTctlMaxDiff (void);

	DWORD getSlamTime (void);
	void setSlamTime (DWORD);

	/*DWORD getAltVidSlamTime (void);
	void setAltVidSlamTime (DWORD);*/

	DWORD getStepUpRampTime (void);
	DWORD getStepDownRampTime (void);
	void setStepUpRampTime (DWORD);
	void setStepDownRampTime (DWORD);

	//HTC Section - Read status
	bool HTCisCapable ();
	bool HTCisEnabled ();
	bool HTCisActive ();
	bool HTChasBeenActive ();
	DWORD HTCTempLimit ();
	bool HTCSlewControl ();
	DWORD HTCHystTemp ();
	DWORD HTCPStateLimit ();
	bool HTCLocked ();

	DWORD getAltVID ();

	//HTC Section - Change status
	void HTCEnable ();

	void HTCDisable ();
	void HTCsetTempLimit (DWORD);
	void HTCsetHystLimit (DWORD);
	void setAltVid (DWORD);
		
	//PSI_L bit
	bool getPsiEnabled ();
	DWORD getPsiThreshold ();
	void setPsiEnabled (bool);
	void setPsiThreshold (DWORD);

	//HyperTransport Section
	void setHTLinkSpeed (DWORD, DWORD);

	//Various settings
	bool getC1EStatus ();
	void setC1EStatus (bool);

	// Autocheck mode
	void checkMode ();

	//Performance counters
	void perfCounterGetInfo ();
	void perfCounterGetValue (unsigned int);
	void perfMonitorCPUUsage ();

	//Scaler helper methods
	void getCurrentStatus (struct procStatus *pStatus, DWORD core);

};

