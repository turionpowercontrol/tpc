#pragma once

#define MAX_CORES (sizeof(unsigned int *)<<3) //MAX_CORES depends on system architecture
#define MAX_NODES (sizeof(DWORD)<<3) //MAX_NODES is fixed to 32
#define PROCESSORMASK DWORD_PTR

#ifdef _WIN32

#ifndef uint64_t
#define uint64_t long long unsigned int
#endif

#include <windows.h>
#include "OlsApi.h"
#include "OlsDef.h"
#include "MSVC_Round.h"
#include <tchar.h>
#endif

#ifdef __linux
#include "cpuPrimitives.h"
#endif

#include <math.h>

//MSRs defines
//Base (pstate 0) MSR register for Family 11h processors:
#define BASE_ZM_PSTATEMSR 0xC0010064

//Base (pstate 0) MSR register for Family 10h processors:
#define BASE_K10_PSTATEMSR 0xC0010064

//Shared (both Family 10h and Family 11h use the same registers)
//regarding PSTATE Control, COFVID Status and CMPHALT registers.
#define BASE_PSTATE_CTRL_REG 0xC0010062
#define COFVID_STATUS_REG 0xC0010071
#define CMPHALT_REG 0xc0010055

//Shared (both Family 10h and Family 11h use the same registers)
//regarding Performance Registers and Time stamp counter
#define BASE_PESR_REG 0xc0010000	
#define BASE_PERC_REG 0xc0010004
#define TIME_STAMP_COUNTER_REG 0x00000010

//Performance Event constants (used for IDLE counting for CPU Usage
//counter)
#define IDLE_COUNTER_EAX 0x430076
#define IDLE_COUNTER_EDX 0x0

//PCI Registers defines for northbridge
#define PCI_FUNC_LINK_CONTROL 0x4
#define PCI_FUNC_MISC_CONTROL_3 0x3
#define PCI_FUNC_DRAM_CONTROLLER 0x2
#define PCI_FUNC_ADDRESS_MAP 0x1
#define PCI_FUNC_HT_CONFIG 0x0
#define PCI_DEV_NORTHBRIDGE 0x18

//Processor Identifier defines
#define ERROR_CLASS_PROCESSOR 0
#define UNKNOWN_PROCESSOR 1
#define TURION_ULTRA_ZM_FAMILY 2
#define TURION_X2_RM_FAMILY 3
#define ATHLON_X2_QL_FAMILY 4
#define SEMPRON_SI_FAMILY 5

#define PROCESSOR_10H_FAMILY 6

//Scaler helper structures:
	struct procStatus {
	DWORD pstate;DWORD vid;DWORD fid;DWORD did;
};



class PState {
	DWORD pstate;
public:
	PState(DWORD);DWORD getPState();
	void setPState(DWORD);
};

class Processor {
protected:

	/*
	 *	Attributes
	 */

	DWORD powerStates;DWORD processorCores;
	char processorStrId[32];DWORD processorIdentifier;DWORD processorNodes; // count of physical processor nodes (eg: 8 on a quad 6100 opteron box).

	//Processor Specs
	int familyBase;
	int model;

	int stepping;
	int familyExtended;
	int modelExtended;
	int brandId;
	int processorModel;
	int string1;
	int string2;
	int pkgType;

	DWORD selectedCore;
	DWORD selectedNode;

	/*
	 *	Methods
	 */

	void setProcessorStrId(const char *);
	void setPowerStates(DWORD);
	//void setProcessorCores (DWORD); //Gone public to allow processor cores forcing from main program
	void setProcessorIdentifier(DWORD);
	void setProcessorNodes(DWORD);

	PROCESSORMASK getMask (DWORD, DWORD);
	PROCESSORMASK getMask ();
	DWORD getNodeMask (DWORD);
	DWORD getNodeMask ();
	bool isValidNode (DWORD);
	bool isValidCore (DWORD);

	//Set method for processor specifications
	void setSpecFamilyBase(int);
	void setSpecModel(int);
	void setSpecStepping(int);
	void setSpecFamilyExtended(int);
	void setSpecModelExtended(int);
	void setSpecBrandId(int);
	void setSpecProcessorModel(int);
	void setSpecString1(int);
	void setSpecString2(int);
	void setSpecPkgType(int);

	virtual void setPCtoIdleCounter(int, int) {
		return;
	}

public:

	const static DWORD ALL_NODES=-1;
	const static DWORD ALL_CORES=-1;

	//Sets the current node to operate on
	void setNode (DWORD);
	//Returns the current node that is operating on
	DWORD getNode ();

	//Sets the current core to operate on
	void setCore (DWORD);
	//Returns the current core that is operating on
	DWORD getCore ();

	//This method is used to know if a module supports the currently installed
	//processor. Main can ask each module if it detects a supported processor
	//in turn and then, if it get a positive answer, it can retrieve processor 
	//family and details
	static bool isProcessorSupported();

	//Public method to show on the console some processor family specific information. This is
	//per-family setting because different processor families have some different
	//specifications.
	virtual void showFamilySpecs(void);

	//Public method to show on the console some DRAM Timings
	virtual void showDramTimings(void);

	//Public method to show some detailed information about Hypertransport Link
	virtual void showHTLink (void);

	//Public method to show some detailed information about Hardware Thermal Control
	virtual void showHTC (void);

	//Public method to override number of cores by main
	void setProcessorCores(DWORD);

	//Get methods to obtain general processor specifications
	int getSpecFamilyBase();
	int getSpecModel();
	int getSpecStepping();
	int getSpecFamilyExtended();
	int getSpecModelExtended();
	int getSpecBrandId();
	int getSpecProcessorModel();
	int getSpecString1();
	int getSpecString2();
	int getSpecPkgType();

	virtual float convertVIDtoVcore(DWORD);
	virtual DWORD convertVcoretoVID(float);
	virtual DWORD convertFDtoFreq(DWORD, DWORD);
	virtual void convertFreqtoFD(DWORD, int *, int *);

	DWORD HTLinkToFreq(DWORD);

	DWORD getProcessorCores() {
		return processorCores;
	}
	DWORD getPowerStates() {
		return powerStates;
	}
	DWORD getProcessorIdentifier() {
		return processorIdentifier;
	}
	char *getProcessorStrId() {
		return processorStrId;
	}
	DWORD getProcessorNodes() {
		return processorNodes;
	}

	//Low level functions to set specific processor primitives
	virtual void setVID (PState , DWORD);
	virtual void setFID (PState , DWORD);
	virtual void setDID (PState , DWORD);

	virtual DWORD getVID(PState);
	virtual DWORD getFID(PState);
	virtual DWORD getDID(PState);

	//Higher level functions that do conversions into lower level primitives
	virtual void setFrequency(PState, DWORD);
	virtual void setVCore(PState, float);

	virtual DWORD getFrequency(PState);
	virtual float getVCore(PState);

	virtual void pStateEnable(PState);
	virtual void pStateDisable(PState);
	virtual bool pStateEnabled(PState);

	//Primitives to set maximum p-state
	virtual void setMaximumPState(PState);

	virtual PState getMaximumPState();

	//Family 11h have a shared northbridge vid for all cores
	virtual DWORD getNBVid();
	virtual void setNBVid(DWORD);

	//Family 10h require northbridge vid per each pstate
	virtual DWORD getNBVid(PState, DWORD);
	virtual void setNBVid(PState, DWORD);

	//Northbridge DID and FID are available only on family 10h processors
	virtual DWORD getNBDid(PState);
	virtual void setNBDid(PState, DWORD);
	virtual DWORD getNBFid();
	virtual void setNBFid(DWORD);

	virtual DWORD getMaxNBFrequency();

	//Family 11h should not implement this
	//Family 10h may report 0-PVI mode or 1-SVI mode
	virtual bool getPVIMode();

	virtual void forcePState(PState);
	virtual bool getSMAF7Enabled();
	virtual DWORD c1eDID();
	virtual DWORD minVID();
	virtual DWORD maxVID();
	virtual DWORD startupPState();
	virtual DWORD maxCPUFrequency(); // 0 means that there
		//is no maximum CPU frequency, i.e. unlocked multiplier

	//Temperature registers
	virtual DWORD getTctlRegister(void);

	virtual DWORD getTctlMaxDiff(void);

	//Voltage slamming time registers
	virtual DWORD getSlamTime(void);
	virtual void setSlamTime(DWORD);

	virtual DWORD getAltVidSlamTime(void);
	virtual void setAltVidSlamTime(DWORD);

	//Voltage ramping time registers - Taken from Phenom Datasheets, not official on turions
	virtual DWORD getStepUpRampTime(void);
	virtual DWORD getStepDownRampTime(void);

	virtual void setStepUpRampTime(DWORD);
	virtual void setStepDownRampTime(DWORD);

	//HTC Section
	virtual bool HTCisCapable();
	virtual bool HTCisEnabled();
	virtual bool HTCisActive();
	virtual bool HTChasBeenActive();
	virtual DWORD HTCTempLimit();
	virtual bool HTCSlewControl();
	virtual DWORD HTCHystTemp();
	virtual DWORD HTCPStateLimit();
	virtual bool HTCLocked();
	virtual DWORD getAltVID();

	//HTC Section - Change status

	virtual void HTCEnable();
	virtual void HTCDisable();
	virtual void HTCsetTempLimit(DWORD);
	virtual void HTCsetHystLimit(DWORD);
	virtual void setAltVid(DWORD);

	//PSI_L bit
	virtual bool getPsiEnabled();
	virtual DWORD getPsiThreshold();
	virtual void setPsiEnabled(bool);
	virtual void setPsiThreshold(DWORD);

	//Hypertransport Section

	virtual void setHTLinkSpeed(DWORD, DWORD);

	virtual void checkMode();

	//Various settings

	virtual bool getC1EStatus();
	virtual void setC1EStatus(bool toggle);

	//performance counters section

	virtual void perfCounterGetInfo();
	virtual void perfCounterGetValue(unsigned int);
	virtual void perfMonitorCPUUsage();


	//Scaler helper methods
    virtual void getCurrentStatus (struct procStatus *, DWORD);


};

