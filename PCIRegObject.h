/*
 * PCIRegObject.h
 *
 *  Created on: 29/mar/2011
 *      Author: paolo
 */

#ifndef PCIREGOBJECT_H_
#define PCIREGOBJECT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "Processor.h"

class PCIRegObject {
private:
	DWORD *reg_ptr;
	DWORD reg;
	DWORD function;
	DWORD device;
	DWORD nodeCount;
	DWORD nodeMask;

	DWORD getPath ();
	DWORD getPath (DWORD, DWORD);

public:
	PCIRegObject();

	void newPCIReg (DWORD, DWORD, DWORD, DWORD);

	bool readPCIReg (DWORD, DWORD, DWORD, DWORD);
	bool writePCIReg ();

	bool setBits (unsigned int, unsigned int, DWORD);
	DWORD getBits (unsigned int, unsigned int, unsigned int);

	virtual ~PCIRegObject();
};

#endif /* PCIREGOBJECT_H_ */
