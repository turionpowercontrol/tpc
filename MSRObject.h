/*
 * MSRObject.h
 *
 *  Created on: 28/mar/2011
 *      Author: paolo
 */

#ifndef MSROBJECT_H_
#define MSROBJECT_H_

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "Processor.h"

class MSRObject {
private:
	DWORD cpuCount;
	PROCESSORMASK cpuMask;
	DWORD reg;
	DWORD *eax_ptr;
	DWORD *edx_ptr;
	unsigned int *absIndex;

public:
	MSRObject();
	bool readMSR (DWORD, PROCESSORMASK);
	bool writeMSR ();
	unsigned int indexToAbsolute (unsigned int);
	uint64_t getBits (unsigned int, unsigned int, unsigned int);
	DWORD getBitsLow (unsigned int, unsigned int, unsigned int);
	DWORD getBitsHigh (unsigned int, unsigned int, unsigned int);
	bool setBits (unsigned int, unsigned int, uint64_t);
	bool setBitsLow (unsigned int, unsigned int, DWORD);
	bool setBitsHigh (unsigned int, unsigned int, DWORD);
	virtual ~MSRObject();
};

#endif /* MSROBJECT_H_ */
