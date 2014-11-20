/*
 * PCIRegObject.cpp
 *
 *  Created on: 29/mar/2011
 *      Author: paolo
 */

#include "PCIRegObject.h"

DWORD PCIRegObject::getPath()
{
	return getPath(this->device, this->function);
}

DWORD PCIRegObject::getPath(DWORD device, DWORD function)
{
	return (device << 3) + (function);
}

//Constructor
PCIRegObject::PCIRegObject()
{
	this->reg_ptr=NULL;
	this->absIndex=NULL;
	this->nodeMask=0x0;
	this->reg=0x0;
	this->function=0x0;
	this->device=0x0;
	this->nodeCount=0x0;
}

/*
 * newPCIReg initializes a new object with default values (zeros).
 * Use this function if you are going to override register values
 */
void PCIRegObject::newPCIReg(DWORD device, DWORD function, DWORD reg, DWORD nodeMask)
{
	DWORD mask;
	unsigned int count;

	this->nodeMask = nodeMask;
	this->reg = reg;
	this->function = function;
	this->device = device;

	//count as many nodes are accounted in nodeMask
	mask = this->nodeMask;
	count = 0;
	while (mask) {
		if (mask & 1) {
			count++;
		}
		mask >>= 1;
	}

	this->nodeCount = count;

	if (this->reg_ptr) free(this->reg_ptr);
	if (this->absIndex) free(this->absIndex);

	this->reg_ptr = (DWORD *) calloc(this->nodeCount, sizeof(DWORD));
	this->absIndex = (unsigned int *) calloc (this->nodeCount, sizeof(unsigned int));

	return;
}

/*
 * readPCIReg reads a PCI Configuration Register
 * Parameters are:
 * 	- device
 * 	- function
 * 	- register
 * 	- nodeMask
 *
 * 	device and function parameters are constructed togheter using the private function
 * 	getPath. In multiprocessor machines, each function is increased for each processor/node
 * 	in the system.
 * 	nodeMask is a bitmask of nodes. Bit 0 will let the function read the PCI register from
 * 	node 0, bit 1 for node 1 and so on.
 */
bool PCIRegObject::readPCIReg(DWORD device, DWORD function, DWORD reg, DWORD nodeMask)
{
	DWORD mask;
	unsigned int count;
	DWORD nid;

	this->nodeMask = nodeMask;
	this->reg = reg;
	this->function = function;
	this->device = device;

	//count as many nodes are accounted in nodeMask
	mask = this->nodeMask;
	count = 0;
	while (mask) {
		if (mask & 1) {
			count++;
		}
		mask >>= 1;
	}

	this->nodeCount = count;

	if (this->reg_ptr) free(this->reg_ptr);
	if (this->absIndex) free (this->absIndex);

	this->reg_ptr = (DWORD *) calloc(this->nodeCount, sizeof(DWORD));
	this->absIndex = (unsigned int *) calloc (this->nodeCount, sizeof(unsigned int));

	mask = this->nodeMask;
	count = 0;
	nid = 0;

	while (mask)
	{
		if (mask & 1)
		{
			if (!ReadPciConfigDwordEx(getPath(this->device+nid, this->function), this->reg, &this->reg_ptr[count]))
			{
				/*This is not needed since memory will be freed by destructor
				free(this->reg_ptr);
				free(this->absIndex);*/
				this->nodeCount = 0;
				return false;
			}
			absIndex[count] = nid;
			count++;
		}

		nid++;
		mask >>= 1;

	}

	return true;
}

/*
 * writePCIReg writes the PCI registers to the processors using nodeMask and parameters
 * set when using readPCIReg.
 */

bool PCIRegObject::writePCIReg ()
{
	DWORD mask;
	unsigned int count;
	DWORD nid;

	if (this->nodeCount==0) return true;

	mask = this->nodeMask;
	count=0;
	nid=0;

	while (mask)
	{
		if (mask & 1)
		{
			if (!WritePciConfigDwordEx (getPath(this->device+nid, this->function),this->reg,this->reg_ptr[count])) return false;
			count++;
		}

		nid++;
		mask >>= 1;

	}

	return true;

}

unsigned int PCIRegObject::indexToAbsolute (unsigned int index)
{

	return this->absIndex[index];

}

/* Returns the number of nodes currently in memory of an object */
DWORD PCIRegObject::getCount ()
{
	return this->nodeCount;
}

/*
 * getBits returns an integer for a specific node. Base and length parameters are used to isolate
 * the sector of the whole register that is interesting.
 */
DWORD PCIRegObject::getBits (unsigned int nodeNumber, unsigned int base, unsigned int length)
{

	DWORD xReg;

	if (this->nodeCount==0) return 0;
	if (nodeNumber>=this->nodeCount) return 0;

	xReg=this->reg_ptr[nodeNumber];

	xReg=xReg<<(32-base-length);
	xReg=xReg>>(32-length);

	return xReg;
}

/*
 * setBits set the bits for all the processors specified in nodeMask.
 * Base and length specify the offset and the width of the sector of the register
 * we're interested in.
 */
bool PCIRegObject::setBits (unsigned int base, unsigned int length, DWORD value)
{

	DWORD mask;
	DWORD count;

	if (this->nodeCount==0) return false;

	mask = -1;
	mask >>= (32 - length);
	mask <<= base;

	value=value<<base;
	value=value & mask;

	mask=~mask;

	for (count=0;count<this->nodeCount;count++)
		this->reg_ptr[count]=(this->reg_ptr[count] & mask) | value;

	return true;

}

PCIRegObject::~PCIRegObject()
{

	if (this->reg_ptr) free (this->reg_ptr);
	if (this->absIndex) free (this->absIndex);

}
