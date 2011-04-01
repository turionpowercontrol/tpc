/*
 * MSRObject.cpp
 *
 *  Created on: 28/mar/2011
 *      Author: paolo
 */

#include "MSRObject.h"

//Constructor: inizializes the object
MSRObject::MSRObject() {
	this->eax_ptr=NULL;
	this->edx_ptr=NULL;
	this->cpuMask=0x0;
	this->reg=0x0;
	this->cpuCount=0x0;

}

/*
 * readMSR: reads the MSR defined in reg parameter with the mask described in cpuMask
 * cpuMask is defined as a bitmask where bit 0 is cpu 0, bit 1 is cpu 1 and so on
 */
bool MSRObject::readMSR (DWORD reg, PROCESSORMASK cpuMask) {

	unsigned int count=0;
	unsigned int pId=0;
	PROCESSORMASK mask;

	this->reg=reg;
	this->cpuMask=cpuMask;

	//count as many processors are accounted in cpuMask
	for (pId=0;pId<MAX_CORES;pId++) {
		mask=(PROCESSORMASK)1<<pId;
		if (cpuMask & mask) count++;
	}

	this->cpuCount=count;

	if (this->eax_ptr) free (this->eax_ptr);
	if (this->edx_ptr) free (this->edx_ptr);

	this->eax_ptr=(DWORD *)calloc (this->cpuCount, sizeof(DWORD));
	this->edx_ptr=(DWORD *)calloc (this->cpuCount, sizeof(DWORD));

	count=0;
	pId=0;

	while (pId<MAX_CORES) {

		mask=(PROCESSORMASK)1<<pId;
		if (cpuMask & mask) {
			if (!RdmsrPx (this->reg,&eax_ptr[count], &edx_ptr[count], mask)) {
				free(this->eax_ptr);
				free(this->edx_ptr);
				this->cpuCount=0;
				return false;
			}
			count++;
		}

		pId++;

	}

	return true;

}

/*
 * writeMSR writes the msr to the processor. Requires no parameters, since the register is
 * defined when readMSR is called and the mask is the same
 *
 */
bool MSRObject::writeMSR () {

	PROCESSORMASK mask;
	DWORD pId;
	unsigned int count;

	if (this->cpuCount==0) return true;

	pId=0;
	count=0;

	while (pId<MAX_CORES) {

		mask=(PROCESSORMASK)1<<pId;

		if (this->cpuMask & mask) {
			if (!WrmsrPx (this->reg, this->eax_ptr[count], this->edx_ptr[count], mask)) return false;
			count++;
		}

		pId++;

	}

	return true;

}

/*
 * getBits return a 64-bit integer containing the part of the MSR with offset defined in base parameter
 * and with length bits. cpuNumber parameter defines the specific cpu. cpuNumber=0 means that you are going
 * to read bits from first cpu in cpuMask, cpuNumber=1 means reading from second cpu in cpuMask and so on.
 */

uint64_t MSRObject::getBits (unsigned int cpuNumber, unsigned int base, unsigned int length) {

	uint64_t xReg;

	if (this->cpuCount==0) return 0;
	if (cpuNumber>=this->cpuCount) return 0;

	xReg=this->eax_ptr[cpuNumber]+((uint64_t)this->edx_ptr[cpuNumber]<<32);

	xReg=xReg<<(64-base-length);
	xReg=xReg>>(64-length);

	return xReg;

}

/*
 * getBitsLow is as getBits, but just on the lower part (eax) of the MSR register.
 * It is preferable to use this on 32 bit systems since it is faster in such environment
 *
 */
DWORD MSRObject::getBitsLow (unsigned int cpuNumber, unsigned int base, unsigned int length) {

	DWORD xReg;

	if (this->cpuCount==0) return 0;
	if (cpuNumber>=this->cpuCount) return 0;

	xReg=this->eax_ptr[cpuNumber];

	xReg=xReg<<(32-base-length);
	xReg=xReg>>(32-length);

	return xReg;
}

/*
 * getBitsLow is as getBits, but just on the lower part (eax) of the MSR register.
 * It is preferable to use this on 32 bit systems since it is faster in such environment
 *
 */
DWORD MSRObject::getBitsHigh (unsigned int cpuNumber, unsigned int base, unsigned int length) {

	DWORD xReg;

	if (this->cpuCount==0) return 0;
	if (cpuNumber>=this->cpuCount) return 0;

	xReg=this->edx_ptr[cpuNumber];

	xReg=xReg<<(32-base-length);
	xReg=xReg>>(32-length);

	return xReg;
}


/*
 * setBits set the bits for all cpu in cpuMask. As usual, base parameter is the offset from least
 * significant bit and length parameter is the length of the supposed part of register you are
 * going to write. value parameter is a 64 bit integer that is the value to be set in the MSR.
 * Note that value parameter is automatically cut to length bits to prevent overlapping in case
 * of overflow to adiacent registers.
 */
bool MSRObject::setBits (unsigned int base, unsigned int length, uint64_t value) {

	uint64_t xReg;
	uint64_t mask;
	DWORD count;

	if (this->cpuCount==0) return false;

	//Does some bitshifting to create a bitmask to isolate
	//the part of the register that is afftected by the change
	mask=-1;
	mask=mask<<(64-base-length);
	mask=mask>>(64-length);
	mask=mask<<base;

	//Bitshifts the value parameter to the right position
	//and then cut it to prevent overlapping on other bits
	value=value<<base;
	value=value & mask;

	//Inverts the mask so we can do an AND on the whole register
	//to isolate the right range of bits
	mask=~mask;

	//printf ("base: %d, length: %d, mask: %llx\n", base, length, mask);

	for (count=0;count<this->cpuCount;count++) {

		xReg=this->eax_ptr[count]+((uint64_t)this->edx_ptr[count]<<32);

		xReg=(xReg & mask) | value;

		this->edx_ptr[count]=(DWORD)(xReg>>32);
		this->eax_ptr[count]=(DWORD)*(&xReg);

	}

	return true;

}

/*
 * setBitsLow is as setBits, but does the job only on lower part (eax) of the MSR register.
 * It uses less resources than setBits and is preferable for use in 32 bit systems.
 *
 */
bool MSRObject::setBitsLow (unsigned int base, unsigned int length, DWORD value) {

	DWORD mask;
	DWORD count;

	if (this->cpuCount==0) return false;

	mask=-1;
	mask=mask<<(32-base-length);
	mask=mask>>(32-length);
	mask=mask<<base;

	value=value<<base;
	value=value & mask;

	mask=~mask;

	for (count=0;count<this->cpuCount;count++)
		this->eax_ptr[count]=(this->eax_ptr[count] & mask) | value;

	return true;

}

/*
 * setBitsHigh is as setBits, but does the job only on higher part (edx) of the MSR register.
 * It uses less resources than setBits and is preferable for use in 32 bit systems.
 *
 */
bool MSRObject::setBitsHigh (unsigned int base, unsigned int length, DWORD value) {

	DWORD mask;
	DWORD count;

	if (this->cpuCount==0) return false;

	mask=-1;
	mask=mask<<(32-base-length);
	mask=mask>>(32-length);
	mask=mask<<base;

	value=value<<base;
	value=value & mask;

	mask=~mask;

	for (count=0;count<this->cpuCount;count++)
		this->edx_ptr[count]=(this->eax_ptr[count] & mask) | value;

	return true;

}

//Releases dynamic memory and destroys the object
MSRObject::~MSRObject() {
	// TODO Auto-generated destructor stub
	if (this->eax_ptr) free (this->eax_ptr);
	if (this->edx_ptr) free (this->edx_ptr);
}
