/*
 * PerformanceCounter.h
 *
 *  Created on: 25/mag/2011
 *      Author: paolo
 */

#ifndef PERFORMANCECOUNTER_H_
#define PERFORMANCECOUNTER_H_

#include "Processor.h"
#include "MSRObject.h"

class PerformanceCounter {
protected:

	//On Family 11h BKDG Manual (doc. 41256 Rev. 3.00) see chapter 3.12, page 224 for reference

	PROCESSORMASK cpuMask;

	char perfCounterSlot[MAX_CORES];

	unsigned char slot;
	unsigned char maxslots;

	unsigned short int eventSelect; //Event selection
	unsigned char counterMask; //Counter Mask
	unsigned char unitMask; //Usually set to 0, can be used to select a sub-event
	bool invertCntMask; //Invert counter mask, check reference for explanation
	bool enableAPICInterrupt; //Enables an APIC Interrupt when a counter overflows
	bool edgeDetect; //0 means Level Detect, 1 means Edge Detect
	bool countOsMode; //Counts events happening in OS Mode
	bool countUserMode; //Counts events happening in User Mode
	unsigned int pesrReg; //Base PESR Register for the CPU
	unsigned int percReg; //Base PERC Register for the CPU
	unsigned char offset; //Register offset from the base register

	bool enabled; //Only used in case of fetch() method. Do not use it elsewhere
	
	unsigned int getPESRReg(unsigned char slot);
	unsigned int getPERCReg(unsigned char slot);

	MSRObject *snapshotRegister;

public:
	PerformanceCounter(PROCESSORMASK cpuMask, DWORD slot, DWORD maxslots);

	bool program ();
	bool fetch(DWORD cpuIndex);
	bool enable ();
	bool disable ();
	bool takeSnapshot ();
	uint64_t getCounter (DWORD cpuIndex);
	unsigned int findAvailableSlot ();
	unsigned int findFreeSlot ();

	virtual ~PerformanceCounter();

	bool getEnabled () const;
	bool getCountUserMode() const;
	unsigned char getCounterMask() const;
	PROCESSORMASK getCpuMask() const;
	bool getEdgeDetect() const;
	bool getEnableAPICInterrupt() const;
	unsigned short int getEventSelect() const;
	bool getInvertCntMask() const;
	unsigned char getSlot() const;
	unsigned char getUnitMask() const;
	void setCountUserMode(bool countUserMode);
	void setCounterMask(unsigned char counterMask);
	void setCpuMask(PROCESSORMASK cpuMask);
	void setEdgeDetect(bool edgeDetect);
	void setEnableAPICInterrupt(bool enableAPICInterrupt);
	void setEventSelect(unsigned short int eventSelect);
	void setInvertCntMask(bool invertCntMask);
	void setSlot(unsigned char slot);
	void setUnitMask(unsigned char unitMask);
	bool getCountOsMode() const;
	void setCountOsMode(bool countOsMode);
	void setMaxSlots(unsigned char maxslots);
};

#endif /* PERFORMANCECOUNTER_H_ */
