/*
 * PerformanceCounter.cpp
 *
 * This class allows a faster use of performance counter.
 * Instructions on how to use:
 *
 * 1 - Instantiate the object giving a cpuMask and a slot to count. Look that the slot
 * 		will be the same for all the cpus in cpuMask
 * 2 - Use setters to setup optional parameters of the performance register
 * 3 - Use program() method to program the performance counter. Program method will program the hardware registers
 * 		but will not enable the performance counters
 * 4 - Use the enable() method to enable the performance counters
 * 5 - Take a snapshot of the current performance counters condition with takeSnapshot() method. This is useful to avoid
 * 		time stretched data retrieving
 * 6 - Obtain freezed counters using getCounter() method for all the cpus you need.
 * 7 - Repeat from step 5 as long as you need the performance counter data
 * 8 - When finished, just issue the disable() method to disable the performance counters. It won't actually deallocate
 * 		but since they are disabled, they can be reused.
 *
 * If you wish to know the actual hardware condition, you can use the fetch () method that reads the hardware registers
 * and changes the protected parameters of this class. Then you can access the parameters via setters/getters.
 *
 *  Created on: 25/mag/2011
 *      Author: paolo
 */

#include "PerformanceCounter.h"

PerformanceCounter::PerformanceCounter(PROCESSORMASK cpuMask, DWORD slot, DWORD maxslots)
{
	if (slot > maxslots)
		this->slot = maxslots;
	else
		this->slot = slot;

	this->maxslots = maxslots;
	this->cpuMask = cpuMask;
	this->eventSelect = 0x00; //Default event
	this->countOsMode = true;
	this->countUserMode = true;
	this->counterMask = 0;
	this->edgeDetect = false;
	this->enableAPICInterrupt = false;
	this->invertCntMask = false;
	this->unitMask = 0;
	
	//Note that the number of slots determines the register to be used
	//Based on BKDG for 15h (pg 547), the legacy slots do exist, however if they are used
	//	you are only receiving the performance info for the lower 4 of 6 slots
	//Therefore the max number of slots can be used to determine the appropriate register
	//If a newer CPU is misdetected/mishandled, the performance info will be correct,
	//	However it will not give you all available info
	if (maxslots == 6)
	{
		this->pesrReg = BASE_PESR_REG_15;
		this->percReg = BASE_PERC_REG_15;
		this->offset = 2;
	}
	else
	{
		this->pesrReg = BASE_PESR_REG;
		this->percReg = BASE_PERC_REG;
		this->offset = 1;
	}

	snapshotRegister = new MSRObject();
}

/*
 * MSR Register is different based on the Extended CPU Family
 * This method will return the register required to get/set the PESR bits
 *
 */

unsigned int PerformanceCounter::getPESRReg(unsigned char slot)
{
	return (this->pesrReg + (this->offset * slot));
}

unsigned int PerformanceCounter::getPERCReg(unsigned char slot)
{
	return (this->percReg + (this->offset * slot));
}

/*
 * program method will program the MS registers according to settings required by the user. This
 * method will not enable the performance counter, will just write the parameters.
 *
 * Returns true if successful, false if not successful.
 *
 */

bool PerformanceCounter::program()
{
	MSRObject *pCounterMSRObject = new MSRObject();
	
	//Loads the current status of the MS registers for all the cpus in the mask
	if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
	{
		free(pCounterMSRObject);
		return false;
	}
	else
		return -2;

	//Programs the bits of the performance counter register according with specifications
	pCounterMSRObject->setBits(8, 8, this->unitMask);
	pCounterMSRObject->setBits(16, 1, this->countUserMode);
	pCounterMSRObject->setBits(17, 1, this->countOsMode);
	pCounterMSRObject->setBits(18, 1, this->edgeDetect);
	pCounterMSRObject->setBits(20, 1, this->enableAPICInterrupt);
	pCounterMSRObject->setBits(23, 1, this->invertCntMask);
	pCounterMSRObject->setBits(24, 8, this->counterMask);
	pCounterMSRObject->setBits(0, 8, this->eventSelect & 0xff); //Lower 8 bits of eventSelect
	pCounterMSRObject->setBits(32, 4, this->eventSelect & 0xf00); //Higher 4 bits of eventSelect
	pCounterMSRObject->setBits(22, 1, 0); //Disables the counter, it must be enabled with another method

	//Writes the data in the MS registers;
	if (!pCounterMSRObject->writeMSR())
	{
		free(pCounterMSRObject);
		return false;
	}

	free(pCounterMSRObject);
	return true;

}

/*
 * fetch method will do the inverse of program method. Program method will program the hardware registers
 * with parameters sets in an object of this class, instead fetch will read the hardware registers and will
 * modify the parameters of the object. Then the parameters can be read by the main program using the
 * getters to obtain actual hardware status.
 *
 * The only limitation with this method is the fact that you can fetch hardware registers of only one cpu
 * in the cpu mask.
 *
 */
bool PerformanceCounter::fetch(DWORD cpuIndex)
{
	MSRObject *pCounterMSRObject = new MSRObject();

	//Loads the current status of the MS registers for all the cpus in the mask.
	//Actually it could be optimized reading data for one cpu only.
	if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
	{
		free(pCounterMSRObject);
		return false;
	}
	else
		return -2;

	this->unitMask=pCounterMSRObject->getBits(cpuIndex, 8, 8);
	this->countUserMode=pCounterMSRObject->getBits(cpuIndex, 16, 1);
	this->countOsMode=pCounterMSRObject->getBits(cpuIndex, 17, 1);
	this->edgeDetect=pCounterMSRObject->getBits(cpuIndex, 18, 1);
	this->enableAPICInterrupt=pCounterMSRObject->getBits(cpuIndex, 20, 1);
	this->invertCntMask=pCounterMSRObject->getBits(cpuIndex, 23, 1);
	this->counterMask=pCounterMSRObject->getBits(cpuIndex, 24, 8);
	this->enabled=pCounterMSRObject->getBits(cpuIndex, 22,1);
	this->eventSelect=pCounterMSRObject->getBits(cpuIndex, 0, 8); //Lower 8 bits of eventSelect
	this->eventSelect+=pCounterMSRObject->getBits(cpuIndex, 32, 4) << 4; //Higher 4 bits of eventSelect

	return true;

}

/*
 * findAvailableSlot() will find a "row" of available slots. It means that it will cycle through the
 * performance counter slots and will search for a slot that is, for all processors in the mask,
 * not enabled at all or has exactly the same parameters that is going to be programmed.
 *
 * Returns the performance counter slot if the class finds an available slot for all the processors.
 * Returns -1 (0xffffffff) if no available slot is found
 * Returns -2 (0xfffffffe) in case of errors
 *
 */

unsigned int PerformanceCounter::findAvailableSlot ()
{
	MSRObject *pCounterMSRObject = new MSRObject();
	unsigned int slot;
	unsigned int cpuIndex;
	bool valid;

	for (slot = 0; slot < this->maxslots; slot++)
	{
		//Loads the current status of the MS registers for all the cpus in the mask.
		if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
		{
			free(pCounterMSRObject);
			return -2;
		}

		valid = true;

		for (cpuIndex = 0; cpuIndex < pCounterMSRObject->getCount(); cpuIndex++)
		{
			//If the counter slot is disabled, we proceed to the next cpu in the mask
			//If the counter slot is enabled, we check the parameters. If we find that parameters are
			//exactly the same as those programmed in the class object, we proceed to the next
			//cpu in the mask. If parameters are not the same, we break the cycle and proceed to the next slot
			if (pCounterMSRObject->getBits(cpuIndex, 22, 1 != 0))
			{
				if ((pCounterMSRObject->getBits(cpuIndex, 8, 8) != this->unitMask) ||
					(pCounterMSRObject->getBits(cpuIndex, 16, 1) != this->countUserMode) ||
					(pCounterMSRObject->getBits(cpuIndex, 17, 1) != this->countOsMode) ||
					(pCounterMSRObject->getBits(cpuIndex, 18, 1) != this->edgeDetect) ||
					(pCounterMSRObject->getBits(cpuIndex, 20, 1) != this->enableAPICInterrupt) ||
					(pCounterMSRObject->getBits(cpuIndex, 23, 1) != this->invertCntMask) ||
					(pCounterMSRObject->getBits(cpuIndex, 24, 8) != this->counterMask) ||
					(pCounterMSRObject->getBits(cpuIndex, 0, 8) != (this->eventSelect & 0xff)) ||
					(pCounterMSRObject->getBits(cpuIndex, 32, 4) != (this->eventSelect & 0xf00)))
				{
						valid=false;
						break;
				}
			}

		}
		
		if (valid == true)
			return slot;
	}
	
	//We found no valid slot, returns -1 as expected
	return -1;

}

/*
 * findFreeSlot() will find a "row" of available slots. It means that it will cycle through the
 * performance counter slots and will search for a slot that is, for all processors in the mask,
 * not enabled at all.
 *
 * Returns the performance counter slot if the class finds free slots for all the processors.
 * Returns -1 (0xffffffff) if no free slot is found
 * Returns -2 (0xfffffffe) in case of errors
 *
 */

unsigned int PerformanceCounter::findFreeSlot ()
{
	MSRObject *pCounterMSRObject = new MSRObject();
	unsigned int slot;
	unsigned int cpuIndex;
	bool valid;

	for (slot = 0; slot < this->maxslots; slot++)
	{
		//Loads the current status of the MS registers for all the cpus in the mask.
		if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
		{
			free(pCounterMSRObject);
			return -2;
		}

		valid=true;

		for (cpuIndex=0;cpuIndex<pCounterMSRObject->getCount();cpuIndex++)
		{
			//If the counter slot is disabled, we proceed to the next cpu in the mask
			//If the counter slos is enabled, we stop the cycle and declare the slot unfree
			if (pCounterMSRObject->getBits(cpuIndex, 22, 1 != 0))
			{
				valid=false;
				break;
			}

		}

		if (valid==true) return slot;

	}

	//We found no valid slot, returns -1 as expected
	return -1;

}


/*
 * Enables the performance counter slot for all the processors in cpuMask
 * Be sure to program them first with program method!
 *
 * Returns true is successful, false in the other case.
 *
 */

bool PerformanceCounter::enable()
{

	MSRObject *pCounterMSRObject = new MSRObject();

	//Loads the current status of the MS registers for all the cpus in the mask.
	if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
	{
		free(pCounterMSRObject);
		return -2;
	}

	pCounterMSRObject->setBitsLow(22, 1, 0x1); //Sets the bit 22, enables the performance counter

	if (!pCounterMSRObject->writeMSR())
	{
		free(pCounterMSRObject);
		return false;
	}

	this->enabled = true;

	free(pCounterMSRObject);
	return true;

}

/*
 * Disables the performance counter slot for all the processors in cpuMask
 *
 *
 * Returns true is successful, false in the other case.
 *
 */

bool PerformanceCounter::disable()
{

	MSRObject *pCounterMSRObject = new MSRObject();
	
	//Loads the current status of the MS registers for all the cpus in the mask.
	if (!pCounterMSRObject->readMSR(getPESRReg(this->slot), this->cpuMask))
	{
		free(pCounterMSRObject);
		return -2;
	}

	pCounterMSRObject->setBitsLow(22, 1, 0x0); //Sets the bit 22, disables the performance counter

	if (!pCounterMSRObject->writeMSR())
	{

		free(pCounterMSRObject);
		return false;

	}

	this->enabled=false;

	free(pCounterMSRObject);
	return true;

}

/*
 * takeSnapshot method will read the counter associated with the performance counter and will store it
 * inside the object. Then the main program can access each single counter for each processor/core without
 * the problem of retrieving time-stretched uncoherent data.
 *
 * Returns true if the snapshot has been taken correctly, else will return false
 */
bool PerformanceCounter::takeSnapshot()
{
	if (!snapshotRegister->readMSR(getPERCReg(this->slot), this->cpuMask))
		return false;

	return true;

}

/*
 * Returns the counter associated with cpuIndex. Remember that cpuIndex is not the absolute number of the cpu
 * you want to read the counter, but it is an index as explained in the getBits method in the MSRObject class
 *
 * Remember also to issue a call to takeSnapshot before calling getCounter, else you will get invalid data
 *
 */
uint64_t PerformanceCounter::getCounter(DWORD cpuIndex)
{
	return snapshotRegister->getBits(cpuIndex, 0, 64);
}

/*
 * Getters and setters, not much interesting
 *
 */

bool PerformanceCounter::getEnabled() const {
	return enabled;
}

bool PerformanceCounter::getCountUserMode() const {
	return countUserMode;
}

unsigned char PerformanceCounter::getCounterMask() const {
	return counterMask;
}

PROCESSORMASK PerformanceCounter::getCpuMask() const {
	return cpuMask;
}

bool PerformanceCounter::getEdgeDetect() const {
	return edgeDetect;
}

bool PerformanceCounter::getEnableAPICInterrupt() const {
	return enableAPICInterrupt;
}

unsigned short int PerformanceCounter::getEventSelect() const {
	return eventSelect;
}

bool PerformanceCounter::getInvertCntMask() const {
	return invertCntMask;
}

unsigned char PerformanceCounter::getSlot() const {
	return slot;
}

unsigned char PerformanceCounter::getUnitMask() const {
	return unitMask;
}

void PerformanceCounter::setCountUserMode(bool countUserMode) {
	this->countUserMode = countUserMode;
}

void PerformanceCounter::setCounterMask(unsigned char counterMask) {
	this->counterMask = counterMask;
}

void PerformanceCounter::setCpuMask(PROCESSORMASK cpuMask) {
	this->cpuMask = cpuMask;
}

void PerformanceCounter::setEdgeDetect(bool edgeDetect) {
	this->edgeDetect = edgeDetect;
}

void PerformanceCounter::setEnableAPICInterrupt(bool enableAPICInterrupt) {
	this->enableAPICInterrupt = enableAPICInterrupt;
}

void PerformanceCounter::setEventSelect(unsigned short int eventSelect) {
	this->eventSelect = eventSelect;
}

void PerformanceCounter::setInvertCntMask(bool invertCntMask) {
	this->invertCntMask = invertCntMask;
}

void PerformanceCounter::setSlot(unsigned char slot) {
	this->slot = slot;
}

bool PerformanceCounter::getCountOsMode() const
{
	return countOsMode;
}

void PerformanceCounter::setCountOsMode(bool countOsMode)
{
	this->countOsMode = countOsMode;
}

void PerformanceCounter::setUnitMask(unsigned char unitMask) {
	this->unitMask = unitMask;
}

void PerformanceCounter::setMaxSlots(unsigned char maxslots)
{
	this->maxslots = maxslots;
}

/*
 * Destructor. Frees resources.
 *
 */

PerformanceCounter::~PerformanceCounter() {

	free(snapshotRegister);

}
