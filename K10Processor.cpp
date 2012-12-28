#include <stdio.h>
#include <stdlib.h>
#include "Signal.h"

#ifdef _WIN32
	#include <windows.h>
	#include "OlsApi.h"
#endif

#ifdef __linux
	#include "cpuPrimitives.h"
	#include <string.h>
#endif

#include "Processor.h"
#include "K10Processor.h"
#include "PCIRegObject.h"
#include "MSRObject.h"
#include "PerformanceCounter.h"

#include "sysdep.h"

//K10Processor class constructor
K10Processor::K10Processor () {

	DWORD eax,ebx,ecx,edx;
	PCIRegObject *pciReg60;
	PCIRegObject *pciReg160;
	bool pciReg60Success;
	bool pciReg160Success;
	DWORD nodes;
	DWORD cores;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("K10Processor::K10Processor - Fatal error during querying for Cpuid(0x1) instruction.\n");
		return;
	}

	int familyBase = (eax & 0xf00) >> 8;
	int model = (eax & 0xf0) >> 4;
	int stepping = eax & 0xf;
	int familyExtended = ((eax & 0xff00000) >> 20)+familyBase;
	int modelExtended = ((eax & 0xf0000) >> 12)+model; /* family 10h: modelExtended is valid */

	boostSupported = 0;
	if (modelExtended == 10) /* revision E */
		boostSupported = 1;

	//Check Brand ID and Package type - CPUID Function 8000_0001 reg EBX
	if (Cpuid(0x80000001,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("K10Processor::K10Processor - Fatal error during querying for Cpuid(0x80000001) instruction.\n");
		return;
	}

	int brandId=(ebx & 0xffff);
	int processorModel=(brandId >> 4) & 0x7f;
	int string1=(brandId >> 11) & 0xf;
	int string2=(brandId & 0xf);
	int pkgType=(ebx >> 28);

	//Sets processor Specs
	setSpecFamilyBase (familyBase);
	setSpecModel (model);
	setSpecStepping (stepping);
	setSpecFamilyExtended (familyExtended);
	setSpecModelExtended (modelExtended);
	setSpecBrandId (brandId);
	setSpecProcessorModel (processorModel);
	setSpecString1 (string1);
	setSpecString2 (string2);
	setSpecPkgType (pkgType);
	setMaxSlots(4);

    // determine the number of nodes, and number of processors.
	// different operating systems have different APIs for doing similiar, but below steps are OS agnostic.

	pciReg60=new PCIRegObject ();
	pciReg160=new PCIRegObject ();

	pciReg60Success=pciReg60->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, 0x60, getNodeMask(0));
	pciReg160Success=pciReg160->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, 0x160, getNodeMask (0));

	if (pciReg60Success && pciReg160Success) {

		nodes=pciReg60->getBits(0,4,3)+1;

		/* TODO: bugbug, physical cores is per processor or it is per system?
		I suppose that the physical cores variable
		should contain the number of cores per-node (or per-processor) and
		not the whole number of cores in the system.
		This happens due to the new PCIRegObject and MSRObject classes.
		*/

		/*physicalCores=pciReg60->getBits(0,16,5);
		physicalCores+=(pciReg160->getBits(0,16,3)<<5);
		physicalCores++;*/

	} else {

		printf ("Warning: unable to detect multiprocessor machine\n");
		nodes=1;

	}

	free (pciReg60);
	free (pciReg160);

	//Check how many physical cores are present - CPUID Function 8000_0008 reg ECX
	if (Cpuid(0x80000008, &eax, &ebx, &ecx, &edx) != TRUE) {
		printf(
				"K10Processor::K10Processor- Fatal error during querying for Cpuid(0x80000008) instruction.\n");
		return;
	}

	cores = (ecx & 0xff) + 1; /* cores per package */

	/*
	 * Normally we assume that nodes per package is always 1 (one physical processor = one package), but
	 * with Magny-Cours chips (modelExtended>=8) this is not true since they share a single package for two
	 * chips (12 cores distributed on a single package but on two nodes).
	 */
	int nodes_per_package = 1;
	if (modelExtended >= 8) {
		PCIRegObject *pci_F3xE8_NbCapReg = new PCIRegObject();
		
		if ((pci_F3xE8_NbCapReg->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xE8, getNodeMask(0))) == TRUE) {
			if (pci_F3xE8_NbCapReg->getBits(0, 29, 1)) {
				nodes_per_package = 2;
			}
		} else {
			printf ("K10Processor::K10Processor - Error discovering nodes per package, results may be unreliable\n");
		}

		free(pci_F3xE8_NbCapReg);
	}

	setProcessorCores(cores/nodes_per_package);
	setProcessorNodes(nodes);
	setNode(0);
	setBoostStates(getNumBoostStates());
	setPowerStates(5);
	setProcessorIdentifier(PROCESSOR_10H_FAMILY);
	setProcessorStrId("Family 10h Processor");

}


/*
 * Static methods to allow external Main to detect current configuration status
 * without instantiating an object. This method that detects if the system
 * has a processor supported by this module
*/
bool K10Processor::isProcessorSupported () {

	DWORD eax;
	DWORD ebx;
	DWORD ecx;
	DWORD edx;

	//Check base CpuID information
	if (Cpuid(0x0,&eax,&ebx,&ecx,&edx)!=TRUE) return false;
	
	//Checks if eax is 0x5 or 0x6. It determines the largest CPUID function available
	//Family 10h returns eax=0x5 if processor revision is D, eax=0x6 if processor revision is E
	if ((eax!=0x5) && (eax!=0x6)) return false;

	//Check "AuthenticAMD" string
	if ((ebx!=0x68747541) || (ecx!=0x444D4163) || (edx!=0x69746E65)) return false;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE) return false;

	int familyBase = (eax & 0xf00) >> 8;
	int familyExtended = ((eax & 0xff00000) >> 20)+familyBase;

	if (familyExtended!=0x10) return false;
	
	//Detects a Family 10h processor, i.e. Phenom, Phenom II, Athlon II, Turion II processors.
	return true;
}

void K10Processor::showFamilySpecs() {
	DWORD psi_l_enable;
	DWORD psi_thres;
	DWORD pstateId;
	unsigned int i, j;
	PCIRegObject *pciRegObject;

	printf("Northbridge Power States table:\n");

	for (j = 0; j < processorNodes; j++) {
		printf("------ Node %d\n", j);
		setNode(j);
		setCore(0);
		for (i = 0; i < getPowerStates(); i++) {
			printf("PState %d - NbVid %d (%0.4fV) NbDid %d NbFid %d\n", i,
					getNBVid(i), convertVIDtoVcore(getNBVid(i)), getNBDid(i),
					getNBFid());
		}

		printf("Northbridge Maximum frequency: ");
		if (getMaxNBFrequency() == 0)
			printf("no maximum frequency, unlocked NB multiplier\n");
		else
			printf("%d\n", getMaxNBFrequency());

		if (getPVIMode()) {
			printf(
					"* Warning: PVI mode is set. Northbridge voltage is used for processor voltage at given pstates!\n");
			printf("* Changing Northbridge voltage changes core voltage too.\n");
		}

		printf("\n");

		for (i = 0; i < getProcessorCores(); i++) {
			setCore(i);
			if (getC1EStatus() == false)
				printf("Core %d C1E CMP halt bit is disabled\n", i);
			else
				printf("Core %d C1E CMP halt bit is enabled\n", i);
		}

		printf("\nVoltage Regulator Slamming time register: %d\n",
				getSlamTime());

		printf("Voltage Regulator Step Up Ramp Time: %d\n", getStepUpRampTime());
		printf("Voltage Regulator Step Down Ramp Time: %d\n",
				getStepDownRampTime());

		pciRegObject = new PCIRegObject();
		if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
			printf("Unable to read PCI Register (0xa0)\n");
		else {
			pstateId = pciRegObject->getBits(0, 16, 12);
			printf("Processor PState Identifier: 0x%x\n", pstateId);
		}

		free(pciRegObject);

		psi_l_enable = getPsiEnabled();

		psi_thres = getPsiThreshold();

		//Parallel VID Interface is available only on family 10h processor
		//when it is allocated on a Single Plane motherboard
		//Family 11h processors and Family 10h processors on Dual Plane
		//motherboards should use Serial Vid Interface
		if (getPVIMode())
			printf(
					"Processor is using Parallel VID Interface (probably Single Plane mode)\n");
		else
			printf(
					"Processor is using Serial VID Interface (probably Dual Plane mode)\n");

		if (psi_l_enable) {
			printf("PSI_L bit enabled (improve VRM efficiency in low power)\n");
			printf("PSI voltage threshold VID: %d (%0.4fV)\n", psi_thres,
					convertVIDtoVcore(psi_thres));
		} else
			printf("PSI_L bit not enabled\n");
	}

}

//Miscellaneous function inherited by Processor abstract class and that
//needs to be reworked for family 10h
float K10Processor::convertVIDtoVcore(DWORD curVid) {

	/*How to calculate VID from Vcore. It doesn't matter if your processor is working in
	 PVI or SVI mode, since processsor register is always 7 bit wide. Then the processor
	 takes care to convert it to Parallel or Serial implementation.

	 Serial VID Interface is simple to calculate.
	 To obtain vcore from VID you need to do:

	 vcore = 1,55 – (VID * 0.0125)

	 The inverse formula to obtain VID from vcore is:

	 vid = (1.55-vcore)/0.0125

	 */

	float curVcore;

	if (getPVIMode()) {
		if (curVid >= 0x5d)
			curVcore = 0.375;
		else {
			if (curVid < 0x3f)
				curVid = (curVid >> 1) << 1;
			curVcore = (float) (1.550 - (0.0125 * curVid));
		}
	} else {
		if (curVid >= 0x7c)
			curVcore = 0;
		else
			curVcore = (float) (1.550 - (0.0125 * curVid));
	}

	return curVcore;
}

DWORD K10Processor::convertVcoretoVID (float vcore) {

	DWORD vid;

	vid=round(((1.55-vcore)/0.0125));
	
	return vid;

}

DWORD K10Processor::convertFDtoFreq (DWORD curFid, DWORD curDid) {
	return (100*(curFid+0x10))/(1<<curDid);
}

void K10Processor::convertFreqtoFD(DWORD freq, int *oFid, int *oDid) {
	/*Needs to calculate the approximate frequency using FID and DID right
	 combinations. Take in account that base frequency is always 200 MHz
	 (that is Hypertransport 1x link speed).

	 For family 10h processor the right formula is:

	 (100*(Fid+16))/(2^Did)

	 Inverse formulas are:

	 fid = (((2^Did) * freq) / 100) - 16

	 did = log2 ((100 * (fid +16))/f)

	 The approach I choose here is to minimize FID and maximize DID.
	 I mean, we start calculating a FID like if DID is 0 and see
	 what FID come out. If FID is strictly less than 0, then it
	 means there's something wrong and we need to raise DID.
	 We raise guessed DID and then calculate again what FID we
	 are getting. If we get a FID that is >=0, then we got
	 a correct FID/DID couple, else we raise again the guessed DID
	 and calculate again, and so on... */

	float fid;
	float did;

	if (freq == 0)
		return;

	did = 0;
	do {

		fid = (((1 << (int) did) * (float) freq) / 100) - 16;

		if (fid < 0)
			did++;

	} while (fid < 0);

	if (fid > 63)
		fid = 63;

	//Actually we don't need to reculate DID, since we guessed a
	//valid one due to the fact that the argument is positive.

	//Debug printf:
	//printf ("\n\nFor frequency %d, FID is %f, DID %f\n", freq, fid, did);

	*oDid = (int) did;
	*oFid = (int) fid;

	return;
}


//-----------------------setVID-----------------------------
//Overloads abstract class setVID to allow per-core personalization
void K10Processor::setVID (PState ps, DWORD vid) {

	MSRObject *msrObject;

	if ((vid>minVID()) || (vid<maxVID())) {
		printf ("K10Processor.cpp: VID Allowed range %d-%d\n", minVID(), maxVID());
		return;
	}

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To set VID, base offset is 9 bits and value is 7 bit wide.
	msrObject->setBitsLow(9,7,vid);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp: unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

//-----------------------setFID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void K10Processor::setFID(PState ps, float floatFid) {

	unsigned int fid;

	MSRObject *msrObject;

	fid=(unsigned int)floatFid;

	if (fid > 63) {
		printf("K10Processor.cpp: FID Allowed range 0-63\n");
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR + ps.getPState(), getMask())) {
		printf("K10Processor.cpp: unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To set FID, base offset is 0 bits and value is 6 bit wide
	msrObject->setBitsLow(0, 6, fid);

	if (!msrObject->writeMSR()) {
		printf("K10Processor.cpp: unable to write MSR\n");
		free(msrObject);
		return;
	}

	free(msrObject);

	return;

}

//-----------------------setDID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void K10Processor::setDID(PState ps, float floatDid) {

	unsigned int did;
	MSRObject *msrObject;

	did=(unsigned int)floatDid;

	if (did > 4) {
		printf("K10Processor.cpp: DID Allowed range 0-4\n");
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR + ps.getPState(), getMask())) {
		printf("K10Processor.cpp: unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To set DID, base offset is 6 bits and value is 3 bit wide
	msrObject->setBitsLow(6, 3, did);

	if (!msrObject->writeMSR()) {
		printf("K10Processor.cpp: unable to write MSR\n");
		free(msrObject);
		return;
	}

	free(msrObject);

	return;

}

//-----------------------getVID-----------------------------

DWORD K10Processor::getVID (PState ps) {

	MSRObject *msrObject;
	DWORD vid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("K10Processor.cpp::getVID - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask.
	//VID is stored after 9 bits of offset and is 7 bits wide
	vid=msrObject->getBitsLow(0, 9, 7);

	free (msrObject);

	return vid;

}

//-----------------------getFID-----------------------------

float K10Processor::getFID (PState ps) {

	MSRObject *msrObject;
	DWORD fid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("K10Processor.cpp::getFID - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//FID is stored after 0 bits of offset and is 6 bits wide
	fid=msrObject->getBitsLow(0, 0, 6);

	free (msrObject);

	return fid;

}

//-----------------------getDID-----------------------------

float K10Processor::getDID (PState ps) {

	MSRObject *msrObject;
	DWORD did;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("K10Processor.cpp::getDID - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//DID is stored after 6 bits of offset and is 3 bits wide
	did=msrObject->getBitsLow(0, 6, 3);

	free (msrObject);

	return (float)did;
}

//-----------------------setFrequency-----------------------------

void K10Processor::setFrequency (PState ps, DWORD freq) {

	int fid, did;

	convertFreqtoFD (freq, &fid, &did);
	
	setFID (ps, (DWORD)fid);
	setDID (ps, (DWORD)did);

	return;
}

//-----------------------setVCore-----------------------------

void K10Processor::setVCore (PState ps, float vcore) {

	DWORD vid;

	vid=convertVcoretoVID (vcore);

	//Check if VID is below maxVID value set by the processor.
	//If it is, then there are no chances the processor will accept it and
	//we reply with an error
	if (vid<maxVID()) {
		printf ("Unable to set vcore: %0.4fV exceeds maximum allowed vcore (%0.4fV)\n", vcore, convertVIDtoVcore(maxVID()));
		return;
	}

	//Again we che if VID is above minVID value set by processor.
	if (vid>minVID()) {
		printf ("Unable to set vcore: %0.4fV is below minimum allowed vcore (%0.4fV)\n", vcore, convertVIDtoVcore(minVID()));
		return;
	}

	setVID (ps,vid);

	return;

}

//-----------------------getFrequency-----------------------------

DWORD K10Processor::getFrequency (PState ps) {

	DWORD curFid, curDid;
	DWORD curFreq;

	curFid=getFID (ps);
	curDid=getDID (ps);

	curFreq=convertFDtoFreq(curFid, curDid);

	return curFreq;

}

//-----------------------getVCore-----------------------------

float K10Processor::getVCore(PState ps) {
	DWORD curVid;
	float curVcore;

	curVid = getVID(ps);

	curVcore = convertVIDtoVcore(curVid);

	return curVcore;
}


bool K10Processor::getPVIMode () {

	PCIRegObject *pciRegObject;
	bool pviMode;

	pciRegObject=new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask())) {
		printf ("K10Processor.cpp::getPVIMode - Unable to read PCI register\n");
		return false;
	}

	pviMode=(bool)pciRegObject->getBits(0,8,1);

	free (pciRegObject);

	return pviMode;

}

//PStates enable/disable/peek
void K10Processor::pStateDisable (PState ps) {

	MSRObject *msrObject;

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("K10Processor.cpp::pStateDisable - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31,1,0x0);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp::pStateDisable - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

void K10Processor::pStateEnable (PState ps) {

	MSRObject *msrObject;

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("K10Processor.cpp::pStateEnable - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31,1,0x1);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp:pStateEnable - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

bool K10Processor::pStateEnabled(PState ps) {

	MSRObject *msrObject;
	unsigned int status;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR + ps.getPState(), getMask())) {
		printf("K10Processor.cpp::pStateEnabled - unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//To peek a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	//We consider just the first cpu in cpuMask
	status = msrObject->getBitsHigh(0, 31, 1);

	free(msrObject);

	if (status == 0)
		return false;
	else
		return true;

}

void K10Processor::setMaximumPState (PState ps) {

	PCIRegObject *pciRegObject;

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask())) {
		printf ("K10Processor.cpp::setMaximumPState - unable to read PCI register\n");
		free (pciRegObject);
		return;
	}

	/*
	 * Maximum pstate is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xdc
	 * bits from 8 to 10
	 */
	pciRegObject->setBits(8,3,ps.getPState());

	if (!pciRegObject->writePCIReg()) {
		printf ("K10Processor.cpp::setMaximumPState - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;

}

PState K10Processor::getMaximumPState () {

	PCIRegObject *pciRegObject;
	PState pState (0);

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask())) {
		printf ("K10Processor.cpp::getMaximumPState - unable to read PCI register\n");
		free (pciRegObject);
		return 0;
	}

	/*
	 * Maximum pstate is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xdc
	 * bits from 8 to 10
	 */
	pState.setPState(pciRegObject->getBits(0, 8 ,3));

	free (pciRegObject);

	return pState;

}

void K10Processor::setNBVid (PState ps, DWORD nbvid) {

	MSRObject *msrObject;

	if (nbvid > 127) {
		printf ("K10Processor.cpp::setNBVid - Northbridge VID Allowed range 0-127\n");
		return;
	}

	msrObject = new MSRObject();

	/*
	 * Northbridge VID for a specific pstate must be set for all cores
	 * of a single node. In SVI systems, it should also be coherent with
	 * pstates which have DID=0 or DID=1 (see MSRC001_0064-68 pag. 327 of
	 * AMD Family 10h Processor BKDG document). We don't care here of this
	 * fact, this is a user concern.
	 */

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask(ALL_NODES, selectedNode))) {
		printf ("K10Processor::setNBVid - Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge VID is stored in low half of MSR register (eax) in bits from 25 to 31
	msrObject->setBitsLow(25,7,nbvid);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor::setNBVid - Unable to write MSR\n");
		free (msrObject);
		return;
	}
	
	free (msrObject);

	return;

}

void K10Processor::setNBDid (PState ps, DWORD nbdid) {

	MSRObject *msrObject;

	msrObject=new MSRObject ();

	if ((nbdid!=0) && (nbdid!=1)) {
		printf ("Northbridge DID must be 0 or 1\n");
		return;
	}

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask(ALL_CORES, selectedNode))) {
		printf ("K10Processor::setNBDid - Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge DID is stored in low half of MSR register (eax) in bit 22
	msrObject->setBitsLow(22,1,nbdid);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor::setNBDid - Unable to write MSR\n");
		free (msrObject);
		return;
	}
	
	free (msrObject);

	return;

}

/*
 *MaxNBFrequency is stored per-node, even though it is stored in a
 *MSR and is available per-core
 */

DWORD K10Processor::getMaxNBFrequency() {

	MSRObject *msrObject;
	DWORD maxNBFid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("K10Processor::getMaxNBFrequency - Unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Maximum Northbridge FID is stored in COFVID_STATUS_REG in higher half
	//of register (edx) in bits from 27 to 31
	maxNBFid = msrObject->getBitsHigh(0, 27, 5);

	//If MaxNbFid is equal to 0, then there are no limits on northbridge frequency
	//and so there are no limits on northbridge FID value.
	if (maxNBFid == 0)
		return 0;
	else
		return (maxNBFid + 4) * 200;

}

void K10Processor::forcePState (PState ps) {

	MSRObject *msrObject;

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_PSTATE_CTRL_REG, getMask ())) {
		printf ("K10Processor.cpp::forcePState - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To force a pstate, we act on setting the first 3 bits of register. All other bits must be zero
	msrObject->setBitsLow(0,32,0x0);
	msrObject->setBitsHigh(0,32,0x0);
	msrObject->setBitsLow(0,3,ps.getPState());

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp::forcePState - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;


}

DWORD K10Processor::getNBVid(PState ps) {

	MSRObject *msrObject;
	DWORD nbVid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR + ps.getPState(),
			getMask())) {
		printf("K10Processor::getNBVid - Unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Northbridge VID is stored in low half of MSR register (eax) in bits from 25 to 31
	nbVid = msrObject->getBitsLow(0, 25, 7);

	free(msrObject);

	return nbVid;

}

DWORD K10Processor::getNBDid (PState ps) {

	MSRObject *msrObject;
	DWORD nbDid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR + ps.getPState(),
			getMask())) {
		printf("K10Processor::getNBDid - Unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Northbridge DID is stored in low half of MSR register (eax) in bit 22
	nbDid = msrObject->getBitsLow(0, 22, 1);

	free(msrObject);

	return nbDid;
}

DWORD K10Processor::getNBFid () {

	PCIRegObject *pciRegObject;
	DWORD nbFid;

	pciRegObject=new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,0xd4,getNodeMask())) {
		printf ("K10Processor::getNBFid - Unable to read PCI register\n");
		free (pciRegObject);
		return false;
	}

	/*Northbridge FID is stored in
	 *
	 * device PCI_DEV_NORTHBRIDGE
	 * function PCI_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 0 to 4
	 */

	nbFid=pciRegObject->getBits(0,0,5);

	free (pciRegObject);
	
	return nbFid;

}


void K10Processor::setNBFid(DWORD fid) {

	PCIRegObject *pciRegObject;
	unsigned int i;

	if (fid > 0x1b) {
		printf("setNBFid: fid value must be between 0 and 27\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd4, getNodeMask())) {
		printf("K10Processor::setNBFid - Unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*Northbridge FID is stored in
	 *
	 * device PCI_DEV_NORTHBRIDGE
	 * function PCI_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 0 to 4
	 */

	for (i = 0; i < pciRegObject->getCount(); i++) {
		unsigned int current = pciRegObject->getBits(i, 0, 5);
			
		printf("Node %u: current nbfid: %u (%u MHz), target nbfid: %u (%u MHz)\n",
			pciRegObject->indexToAbsolute(i), current, (current + 4) * 200, fid, (fid + 4) * 200);
	}

	pciRegObject->setBits(0, 5, fid);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::setNBFid - Unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return ;

	/*

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 24 (0x18), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 25 (0x19), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 26 (0x1A), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 27 (0x1B), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 28 (0x1C), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 29 (0x1D), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 30 (0x1E), function 0 (0x00)

	 Description			Host Bridge
	 Location			bus 0 (0x00), device 31 (0x1F), function 0 (0x00)

	 from amd_dev_guide.pdf
	 bits 4:0
	 NbFid: Northbridge frequency ID. Read-write. Cold reset: value varies by product. After a cold
	 reset, this specifies the FID at which the NB is designed to operate. After a warm or cold reset, the NB
	 FID may or may not be reflected in this field, based on the state of NbFidEn. The NB FID may be
	 updated to the value of this field through a warm or cold reset if NbFidEn=1. If that has occurred, then
	 the NB COF is specified by:
	 • NB COF = 200 MHz * (F3xD4[NbFid] + 4h) / (2^MSRC001_00[68:64][NbDid]).
	 This field must be programmed to the requirements specified in MSRC001_0071[MaxNbFid] and
	 must be less than or equal to 1Bh, otherwise undefined behavior results. This field must be
	 programmed to the same value for all nodes in the coherent fabric as specified by 2.4.2.9 [BIOS
	 Northbridge COF and VID Configuration]. See 2.4.2 [P-states]. BIOS must not change the NbFid
	 after enabling the DRAM controller.
	 */

}

//minVID is reported per-node, so selected core is always discarded
DWORD K10Processor::minVID () {

	MSRObject *msrObject;
	DWORD minVid;

	msrObject=new MSRObject;

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf ("K10Processor::minVID - Unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//minVid is stored in COFVID_STATUS_REG in high half register (edx)
	//from bit 10 to bit 16
	minVid=msrObject->getBitsHigh(0,10,7);

	free (msrObject);

	//If minVid==0, then there's no minimum vid.
	//Since the register is 7-bit wide, then 127 is
	//the maximum value allowed.
	if (getPVIMode()) {
		//Parallel VID mode, allows minimum vcore VID up to 0x5d
		if (minVid==0) return 0x5d; else return minVid;
	} else {
		//Serial VID mode, allows minimum vcore VID up to 0x7b
		if (minVid==0) 	return 0x7b; else return minVid;
	}
}

//maxVID is reported per-node, so selected core is always discarded
DWORD K10Processor::maxVID() {
	MSRObject *msrObject;
	DWORD maxVid;

	msrObject = new MSRObject;

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("K10Processor::maxVID - Unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//maxVid is stored in COFVID_STATUS_REG in high half register (edx)
	//from bit 3 to bit 9
	maxVid = msrObject->getBitsHigh(0, 3, 7);

	free(msrObject);

	//If maxVid==0, then there's no maximum set in hardware
	if (maxVid == 0)
		return 0;
	else
		return maxVid;
}

//StartupPState is reported per-node. Selected core is discarded
DWORD K10Processor::startupPState () {
	MSRObject *msrObject;
	DWORD pstate;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("K10Processor.cpp::startupPState unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//StartupPState has base offset at 0 bits of high register (edx) and is 3 bit wide
	pstate = msrObject->getBitsHigh(0, 0, 3);

	free(msrObject);

	return pstate;

}

DWORD K10Processor::maxCPUFrequency() {

	MSRObject *msrObject;
	DWORD maxCPUFid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("K10Processor.cpp::maxCPUFrequency unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//maxCPUFid has base offset at 17 bits of high register (edx) and is 6 bits wide
	maxCPUFid = msrObject->getBitsHigh(0, 17, 6);

	free(msrObject);

	return maxCPUFid * 100;

}

DWORD K10Processor::getNumBoostStates(void)
{	
	PCIRegObject *boostControl;
	DWORD numBoostStates;

	if (!boostSupported)
		return 0;

	boostControl = new PCIRegObject();

	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("K10Processor::getNumBoostStates unable to read boost control register\n");
		delete boostControl;
		return 0;
	}

	numBoostStates = boostControl->getBits(0, 2, 1);

	delete boostControl;

	return numBoostStates;
}

void K10Processor::setNumBoostStates(DWORD numBoostStates)
{
	PCIRegObject *boostControl;

	if (!boostSupported)
		return;

	boostControl = new PCIRegObject();

	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("K10Processor::setNumBoostStates unable to read boost control register\n");
		delete boostControl;
		return;
	}
	
	if (boostControl->getBits(0, 31, 1))
	{
		printf("Boost Lock Enabled. Cannot edit NumBoostStates\n");
		delete boostControl;
		return;
	}

	if (boostControl->getBits(0, 0, 2))
	{
		printf("Disable boost before changing the number of boost states\n");
		delete boostControl;
		return;
	}

	boostControl->setBits(2, 1, numBoostStates);

	if (!boostControl->writePCIReg())
	{
		printf("K10Processor::setNumBoostStates unable to write PCI Reg\n");
		delete boostControl;
		return;
	}

	setBoostStates(numBoostStates);

	printf("Number of boosted states set to %d\n", numBoostStates);

	delete boostControl; 
}

/*
 * Specifies whether CPB is enabled or disabled
 */
DWORD K10Processor::getBoost(void)
{
	PCIRegObject *boostControl;
	DWORD boostSrc;

	if (!boostSupported)
		return -1;

	boostControl = new PCIRegObject();

	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("K10Processor::getBoost unable to read boost control register\n");
		delete boostControl;
		return -1;
	}

	boostSrc = boostControl->getBits(0, 0, 2);

	delete boostControl;	
	
	if (boostSrc == 3)
		return 1;
	else if (boostSrc == 0)
		return 0;
	else
		return -1;
}

void K10Processor::setBoost(bool boost)
{	
	PCIRegObject *boostControl;

	if (!boostSupported)
		return;

	boostControl = new PCIRegObject();

	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("K10Processor::setBoost unable to read boost control register\n");
		delete boostControl;
		return;
	}
	
	if (boostControl->getBits(0, 31, 1))
	{
		printf("Boost Lock Enabled. NumBoostStates and CStateCnt are read-only.\n");
	}
	else
	{
		printf("Boost Lock Disabled. Unlocked processor.\n");
		printf("NumBoostStates and CStateCnt can be modified.\n");
	}

	boostControl->setBits(0, 2, boost ? 3 : 0);

	if (!boostControl->writePCIReg())
	{
		printf("K10Processor::enableBoost unable to write PCI Reg\n");
		delete boostControl;
		return;
	}

	if (boost)
		printf ("Boost enabled\n");
	else
		printf ("Boost disabled\n");

	delete boostControl;
}

//DRAM Timings tweaking ----------------

DWORD K10Processor::setDramTiming(
		DWORD device, // 0 or 1
		DWORD Tcl, DWORD Trcd, DWORD Trp, DWORD Trtp, DWORD Tras, DWORD Trc,
		DWORD Twr, DWORD Trrd, DWORD Tcwl, DWORD T_mode) {

	DWORD T_mode_current;

	bool reg1;
	bool reg2;
	bool reg3;

	PCIRegObject *dramTimingLowRegister;
	PCIRegObject *dramConfigurationHighRegister;
	PCIRegObject *dramMsrRegister;

	//
	// parameter validation.
	//
	if (Tcl < 4 || Tcl > 12) {
		printf("Tcl out of allowed range (4-12)\n");
		return false;
	}

	if (Trcd < 5 || Trcd > 12) {
		printf("Trcd out of allowed range (5-12)\n");
		return false;
	}

	if (Trp < 5 || Trp > 12) {
		printf("Trp out of allowed range (5-12)\n");
		return false;
	}

	if (Trtp < 4 || Trtp > 7) {
		printf("Trtp out of allowed range (4-7)\n");
		return false;
	}

	if (T_mode < 1 || T_mode > 2) {
		printf("T out of allowed range (1-2)\n");
		return false;
	}

	/*if (device == 1) {
			DRAMMRSRegister = 0x184;
			DRAMTimingLowRegister = 0x188;
			DRAMConfigurationHighRegister = 0x194;
		} else {
			DRAMMRSRegister = 0x84;
			DRAMTimingLowRegister = 0x88;
			DRAMConfigurationHighRegister = 0x94;
		}
	*/

	dramTimingLowRegister = new PCIRegObject();
	dramConfigurationHighRegister = new PCIRegObject();
	dramMsrRegister = new PCIRegObject();

	if (device == 1) {

		reg1 = dramMsrRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x184, getNodeMask());
		reg2 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x188, getNodeMask());
		reg3 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());

	} else {

		reg1 = dramMsrRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x84, getNodeMask());
		reg2 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x88, getNodeMask());
		reg3 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());

	}

	if (!(reg1 && reg2 && reg3)) {
		printf("K10Processor::setDramTimingLow - unable to read PCI register\n");
		free(dramMsrRegister);
		free(dramTimingLowRegister);
		free(dramConfigurationHighRegister);
		return false;
	}

	/*ReadPciConfigDwordEx(Target, DRAMConfigurationHighRegister,
				&ConfigurationHighResult);*/


	if (dramConfigurationHighRegister->getBits(0,20,1)) {
		T_mode_current = 2;
	} else {
		T_mode_current = 1;
	}

	//ReadPciConfigDwordEx(Target, DRAMTimingLowRegister, &TimingLowResult);

	dramTimingLowRegister->setBits(0,4,(Tcl-4)); //NewTcl = (Tcl - 4) & 0x0f;
	dramTimingLowRegister->setBits(4,3,(Trcd-5)); //NewTrcd = ((Trcd - 5) & 0x07) << 4;
	dramTimingLowRegister->setBits(7,3,(Trp-5)); //NewTrp = ((Trp - 5) & 0x07) << 7;
	dramTimingLowRegister->setBits(10,2,(Trtp-4)); //NewTrtp = ((Trtp - 4) & 0x03) << 10;
	dramTimingLowRegister->setBits(12,4,(Tras-15)); //NewTras = ((Tras - 15) & 0x0F) << 12;
	dramTimingLowRegister->setBits(16,5,(Trc-11)); //NewTrc = ((Trc - 11) & 0x1F) << 16;
	dramTimingLowRegister->setBits(22,2,(Trrd-4)); //NewTrrd = ((Trrd - 4) & 0x03) << 22;

	/*NewTimingLowResult = TimingLowResult & ~((0xF << 0) | (0x7 << 4) | (0x7
			<< 7) | (0x3 << 10) | (0xF << 12) | (0x1F << 16) | (0x3 << 22));

	NewTimingLowResult |= (NewTcl | NewTrcd | NewTrp | NewTrtp | NewTras
			| NewTrc | NewTrrd);*/


	//ReadPciConfigDwordEx(Target, DRAMMRSRegister, &MRSResult);

	dramMsrRegister->setBits(4,3,(Twr-4)); //NewTwr = ((Twr - 4) & 0x07) << 4;
	dramMsrRegister->setBits(20,3,(Tcwl-5)); //NewTcwl = ((Tcwl - 5) & 0x07) << 20;

	/*NewMRSResult = MRSResult & ~((0x07 << 4) | (0x07 << 20));

	NewMRSResult |= (NewTwr | NewTcwl);*/

	//
	// perform relevant updates.
	//

	/*if (NewTimingLowResult != TimingLowResult) {
		printf("Updating DRAM Timing Low Register from %x to %x\n",
				TimingLowResult, NewTimingLowResult);
		WritePciConfigDwordEx(Target, DRAMTimingLowRegister, NewTimingLowResult);
	}*/

	printf ("Updating DRAM Timing Low Register... ");
	if (!dramTimingLowRegister->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");

	/*if (NewMRSResult != MRSResult) {
		printf("Updating DRAM MRS Register from %x to %x\n", MRSResult,
				NewMRSResult);
		WritePciConfigDwordEx(Target, DRAMTimingLowRegister, NewTimingLowResult);
	}*/

	printf ("Updating DRAM MSR Register... ");
	if (!dramMsrRegister->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");

	if (T_mode_current != T_mode) {
		if (T_mode == 2) {
			dramConfigurationHighRegister->setBits(20,1,1); //ConfigurationHighResult |= (1 << 20);
		} else {
			dramConfigurationHighRegister->setBits(20,1,0); //ConfigurationHighResult &= ~(1 << 20);
		}

		printf("Updating T from %uT to %uT... ", T_mode_current, T_mode);

		/*WritePciConfigDwordEx(Target, DRAMConfigurationHighRegister,
				ConfigurationHighResult);*/

		if (!dramConfigurationHighRegister->writePCIReg())
			printf ("failed\n");
		else
			printf ("success\n");
	}

	return true;
}


//Temperature registers ------------------

DWORD K10Processor::getTctlRegister (void) {

	PCIRegObject *pciRegObject;
		DWORD temp;

		pciRegObject = new PCIRegObject();

		if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
				0xa4, getNodeMask())) {
			printf("K10Processor.cpp::getTctlRegister - unable to read PCI register\n");
			free(pciRegObject);
			return 0;
		}

		/*
		 * Tctl data is stored in PCI register with
		 * device PCI_DEV_NORTHBRIDGE
		 * function PC_FUNC_MISC_CONTROL_3
		 * register 0xa4
		 * bits from 21 to 31
		 */
		temp = pciRegObject->getBits(0, 21, 11);

		free(pciRegObject);

		return temp >> 3;
}

DWORD K10Processor::getTctlMaxDiff() {

	PCIRegObject *pciRegObject;
	DWORD maxDiff;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa4, getNodeMask())) {
		printf("K10Processor.cpp::getTctlMaxDiff unable to read PCI register\n");
		free(pciRegObject);
		return 0;
	}

	/*
	 * Tctl Max diff data is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xa4
	 * bits from 5 to 6
	 */
	maxDiff = pciRegObject->getBits(0, 5, 2);

	free(pciRegObject);

	return maxDiff;

}

//Voltage Slamming time
DWORD K10Processor::getSlamTime (void) {
	PCIRegObject *pciRegObject;
	DWORD slamTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd8, getNodeMask())) {
		printf("K10Processor.cpp::getSlamTime unable to read PCI register\n");
		free(pciRegObject);
		return 0;
	}

	/*
	 * voltage slamtime is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd8
	 * bits from 0 to 2
	 */
	slamTime = pciRegObject->getBits(0, 0, 3);

	free(pciRegObject);

	return slamTime;
}

void K10Processor::setSlamTime (DWORD slmTime) {

	PCIRegObject *pciRegObject;

	if (slmTime<0 || slmTime >7) {
		printf ("Invalid Slam Time: must be between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd8,
			getNodeMask())) {
		printf("K10Processor::setSlamTime -  unable to read PCI Register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * voltage slamtime is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd8
	 * bits from 0 to 2
	 */

	pciRegObject->setBits(0, 3, slmTime);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor.cpp::setSlamTime - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free (pciRegObject);

	return;

}

/*
DWORD K10Processor::getAltVidSlamTime (void) {
	DWORD miscReg;
	DWORD vsSlamTime;

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xdc,&miscReg);

	vsSlamTime=(miscReg >> 29) & 0x7;

	return vsSlamTime;
}

void K10Processor::setAltVidSlamTime (DWORD slmTime) {
	DWORD miscReg;

	if (slmTime<0 || slmTime >7) {
		printf ("Invalid AltVID Slam Time: must be between 0 and 7\n");
		return;
	}

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xdc,&miscReg);

	miscReg=(miscReg & 0xE0000000)+ (slmTime<<29);

	WritePciConfigDwordEx (MISC_CONTROL_3,0xdc,miscReg);

} */


//Voltage Ramping time
DWORD K10Processor::getStepUpRampTime (void) {

	PCIRegObject *pciRegObject;
	DWORD vsRampTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4,
			getNodeMask())) {
		printf("K10Processor.cpp::getStepUpRampTime unable to read PCI Register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * power step up ramp time is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 24 to 27
	 */

	vsRampTime=pciRegObject->getBits(0,24,4);

	free (pciRegObject);

	return vsRampTime;

}

DWORD K10Processor::getStepDownRampTime (void) {

	PCIRegObject *pciRegObject;
	DWORD vsRampTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4,
			getNodeMask())) {
		printf("K10Processor::getStepDownRampTime -  unable to read PCI Register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * power step down ramp time is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 20 to 23
	 */

	vsRampTime=pciRegObject->getBits(0,20,4);

	free (pciRegObject);

	return vsRampTime;

}

void K10Processor::setStepUpRampTime (DWORD rmpTime) {

	PCIRegObject *pciRegObject;

		pciRegObject = new PCIRegObject();

		if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4,
				getNodeMask())) {
			printf("K10Processor::setStepUpRampTime unable to read PCI Register\n");
			free(pciRegObject);
			return;
		}

		/*
		 * power step up ramp time is stored in PCI register with
		 * device PCI_DEV_NORTHBRIDGE
		 * function PC_FUNC_MISC_CONTROL_3
		 * register 0xd4
		 * bits from 24 to 27
		 */

		pciRegObject->setBits(24,4,rmpTime);

		if (!pciRegObject->writePCIReg()) {
			printf ("K10Processor::setStepUpRampTime - unable to write PCI register\n");
			free (pciRegObject);
			return;
		}

		free (pciRegObject);

		return;


}

void K10Processor::setStepDownRampTime(DWORD rmpTime) {

	PCIRegObject *pciRegObject;

	if (rmpTime < 0 || rmpTime > 0xf) {
		printf("Invalid Ramp Time: value must be between 0 and 15\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd4, getNodeMask())) {
		printf("K10Processor::setStepDownRampTime - unable to read PCI Register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * power step down ramp time is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 24 to 27
	 */

	pciRegObject->setBits(20, 4, rmpTime);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::setStepDownRampTime - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}


// AltVID - HTC Thermal features

bool K10Processor::HTCisCapable () {
	PCIRegObject *pciRegObject;
	DWORD isCapable;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xe8, getNodeMask())) {
		printf("K10Processor::HTCisCapable - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC-capable bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xe8
	 * bit 10
	 */

	isCapable = pciRegObject->getBits(0, 10, 1);

	free(pciRegObject);

	return (bool) isCapable;
}

bool K10Processor::HTCisEnabled() {
	PCIRegObject *pciRegObject;
	DWORD isEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCisEnabled - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC-enabled bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 0
	 */

	isEnabled = pciRegObject->getBits(0, 0, 1);

	free(pciRegObject);

	return (bool) isEnabled;

}

bool K10Processor::HTCisActive() {
	PCIRegObject *pciRegObject;
	DWORD isActive;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCisActive - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC active bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 4
	 */

	isActive = pciRegObject->getBits(0, 4, 1);

	free(pciRegObject);

	return (bool) isActive;

}

bool K10Processor::HTChasBeenActive () {
	PCIRegObject *pciRegObject;
	DWORD hasBeenActivated;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTChasBeenActive - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC has been activated bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 5
	 */

	hasBeenActivated = pciRegObject->getBits(0, 5, 1);

	free(pciRegObject);

	return (bool) hasBeenActivated;

}

DWORD K10Processor::HTCTempLimit() {
	PCIRegObject *pciRegObject;
	DWORD tempLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCTempLimit - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC temperature limit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bits from 16 to 22
	 */

	tempLimit = 52 + (pciRegObject->getBits(0, 16, 7) >> 1);

	free(pciRegObject);

	return tempLimit;

}

bool K10Processor::HTCSlewControl() {
	PCIRegObject *pciRegObject;
	DWORD slewControl;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCSlewControl - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC slew Control bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 23
	 */

	slewControl = pciRegObject->getBits(0, 23, 1);

	free(pciRegObject);

	return (bool) slewControl;

}

DWORD K10Processor::HTCHystTemp() {
	PCIRegObject *pciRegObject;
	DWORD hystTemp;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCHystTemp - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC Hysteresis Temperature is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bits from 24 to 27
	 */

	hystTemp = pciRegObject->getBits(0, 24, 4) >> 1;

	free(pciRegObject);

	return hystTemp;

}

DWORD K10Processor::HTCPStateLimit () {

	PCIRegObject *pciRegObject;
	DWORD pStateLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCPStateLimit - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC PState limit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bits from 28 to 30
	 */

	pStateLimit = pciRegObject->getBits(0, 28, 3);

	free(pciRegObject);

	return pStateLimit;

}

bool K10Processor::HTCLocked() {
	PCIRegObject *pciRegObject;
	DWORD htcLocked;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCLocked - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * HTC locked bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 31
	 */

	htcLocked = pciRegObject->getBits(0, 31, 1);

	free(pciRegObject);

	return (bool) htcLocked;
}

void K10Processor::HTCEnable() {
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCEnable - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * HTC-enabled bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 0
	 */

	pciRegObject->setBits(0, 1, 1);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::HTCEnable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void K10Processor::HTCDisable() {
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCDisable - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * HTC-enabled bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bit 0
	 */

	pciRegObject->setBits(0, 1, 0);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::HTCDisable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

void K10Processor::HTCsetTempLimit (DWORD tempLimit) {

	PCIRegObject *pciRegObject;

	if (tempLimit < 52 || tempLimit > 115) {
		printf("HTCsetTempLimit: accepted range between 52 and 115\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCsetTempLimit - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * HTC temp limit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bits from 16 to 22
	 */

	pciRegObject->setBits(16, 7, (tempLimit - 52) << 1);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::HTCsetTempLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

void K10Processor::HTCsetHystLimit(DWORD hystLimit) {

	PCIRegObject *pciRegObject;

	if (hystLimit < 0 || hystLimit > 7) {
		printf("HTCsetHystLimit: accepted range between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("K10Processor::HTCsetHystLimit - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * HTC temp limit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0x64
	 * bits from 24 to 27
	 */

	pciRegObject->setBits(24, 4, hystLimit << 1);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor::HTCsetHystLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

DWORD K10Processor::getAltVID() {

	PCIRegObject *pciRegObject;
	DWORD altVid;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xdc, getNodeMask())) {
		printf("K10Processor.cpp::getAltVID - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * AltVid is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xDC
	 * bits from 0 to 6
	 */

	altVid = pciRegObject->getBits(0, 0, 7);

	free(pciRegObject);

	return altVid;
}

void K10Processor::setAltVid(DWORD altVid) {

	PCIRegObject *pciRegObject;

	if ((altVid < maxVID()) || (altVid > minVID())) {
		printf("setAltVID: VID Allowed range %d-%d\n", maxVID(), minVID());
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xdc, getNodeMask())) {
		printf("K10Processor.cpp::setAltVID - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * AltVid is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xDC
	 * bits from 0 to 6
	 */

	pciRegObject->setBits(0, 7, altVid);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor.cpp::setAltVID - unable to write to PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

// Hypertransport Link

//TODO: All hypertransport Link section must be tested and validated!!

DWORD K10Processor::getHTLinkWidth(DWORD link, DWORD Sublink, DWORD *WidthIn,
		DWORD *WidthOut, bool *pfCoherent, bool *pfUnganged) {
	DWORD FUNC_TARGET;

	PCIRegObject *linkTypeRegObject;
	PCIRegObject *linkControlRegObject;
	PCIRegObject *linkExtControlRegObject;

	*WidthIn = 0;
	*WidthOut = 0;
	*pfCoherent = FALSE;

	if (Sublink == 1)
		FUNC_TARGET = PCI_FUNC_LINK_CONTROL; //function 4
	else
		FUNC_TARGET = PCI_FUNC_HT_CONFIG; //function 0

	linkTypeRegObject = new PCIRegObject();
	//Link Type Register is located at 0x98 + 0x20 * link
	if (!linkTypeRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			0x98 + (0x20 * link), getNodeMask())) {
		printf(
				"K10Processor::getHTLinkWidth - unable to read linkType PCI Register\n");
		free(linkTypeRegObject);
		return false;
	}

	linkControlRegObject = new PCIRegObject();
	//Link Control Register is located at 0x84 + 0x20 * link
	if (!linkControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			0x84 + (0x20 * link), getNodeMask())) {
		printf(
				"K10Processor::getHTLinkWidth - unable to read linkControl PCI Register\n");
		free(linkTypeRegObject);
		free(linkControlRegObject);
		return false;
	}

	linkExtControlRegObject = new PCIRegObject();
	//Link Control Extended Register is located at 0x170 + 0x04 * link
	if (!linkExtControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			0x170 + (0x04 * link), getNodeMask())) {
		printf(
				"K10Processor::getHTLinkWidth - unable to read linkExtendedControl PCI Register\n");
		free(linkTypeRegObject);
		free(linkControlRegObject);
		free(linkExtControlRegObject);
		return false;
	}

	//
	// determine if the link is connected.
	// TODO check LinkConPend == 0 first.
	//

	//Bit 2 says if link is coherent
	if (linkTypeRegObject->getBits(0, 2, 1) == 0)
		*pfCoherent = TRUE;
	else
		*pfCoherent = FALSE;

	//Bit 0 says if link is connected
	if (linkTypeRegObject->getBits(0, 0, 1) == 0) {
		free(linkTypeRegObject);
		free(linkControlRegObject);
		free(linkExtControlRegObject);

		return 0;
	}

	//Bits 28-30 from link Control Register represent output link width
	int Out = linkControlRegObject->getBits(0, 28, 3);
	//Bits 24-26 from link Control Register represent input link width
	int In = linkControlRegObject->getBits(0, 24, 3);

	switch (Out) {
	case 0:
		*WidthOut = 8;
		break;

	case 1:
		*WidthOut = 16;
		break;

	case 7:
		*WidthOut = 0;
		break;

	default:
		*WidthOut = 0;
		break;
	}

	switch (In) {
	case 0:
		*WidthIn = 8;
		break;

	case 1:
		*WidthIn = 16;
		break;

	case 7:
		*WidthIn = 0;
		break;

	default:
		*WidthIn = 0;
		break;
	}

	if (Sublink == 0) {
		//Bit 1 from link Extended Control Register represent if Sublink is ganged or unganged
		if ((linkExtControlRegObject->getBits(0, 0, 1)) == 0)
			*pfUnganged = TRUE;
		else
			*pfUnganged = FALSE;
	}

	free(linkTypeRegObject);
	free(linkControlRegObject);
	free(linkExtControlRegObject);

	return 0;
}

DWORD K10Processor::getHTLinkSpeed (DWORD link, DWORD Sublink) {

	DWORD FUNC_TARGET;

	PCIRegObject *linkRegisterRegObject = new PCIRegObject();
	DWORD linkFrequencyRegister = 0x88;

	DWORD dwReturn;

	if( Sublink == 1 )
		FUNC_TARGET=PCI_FUNC_LINK_CONTROL; //function 4
	else
		FUNC_TARGET=PCI_FUNC_HT_CONFIG; //function 0

	linkFrequencyRegister += (0x20 * link);

	if (!linkRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE,FUNC_TARGET,linkFrequencyRegister,getNodeMask())) {
		printf ("K10Processor::getHTLinkSpeed - unable to read linkRegister PCI Register\n");
		free (linkRegisterRegObject);
		return false;
	}

	dwReturn = linkRegisterRegObject->getBits(0,8,4); //dwReturn = (miscReg >> 8) & 0xF;

	if (getSpecModelExtended() >= 8) { /* revision D or later */
		DWORD linkFrequencyExtensionRegister = 0x9c;
		PCIRegObject *linkExtRegisterRegObject = new PCIRegObject();

		linkFrequencyExtensionRegister += (0x20 * link);

		if (!linkExtRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
				linkFrequencyExtensionRegister, getNodeMask())) {
			printf ("K10Processor::getHTLinkSpeed - unable to read linkExtensionRegister PCI Register\n");
			free(linkRegisterRegObject);
			free(linkExtRegisterRegObject);
			return false;
		}

		//ReadPciConfigDwordEx (Target,LinkFrequencyExtensionRegister,&miscRegExtended);
		//if(miscRegExtended & 1)
		if (linkExtRegisterRegObject->getBits(0,0,1))
		{
			dwReturn |= 0x10;
		}
		free(linkExtRegisterRegObject);
	}

	// 88, 9c
	// a8, bc
	// c8, dc
	// e8, fc
	//

	return dwReturn;
}

void K10Processor::printRoute(DWORD route) {

	if (route & 0x1) {
		printf("this ");
	}

	if (route & 0x2) {
		printf("l0 s0 ");
	}

	if (route & 0x4) {
		printf("l1 s0 ");
	}

	if (route & 0x8) {
		printf("l2 s0 ");
	}

	if (route & 0x10) {
		printf("l3 s0 ");
	}

	if (route & 0x20) {
		printf("l0 s1 ");
	}

	if (route & 0x40) {
		printf("l1 s1 ");
	}

	if (route & 0x80) {
		printf("l2 s1 ");
	}

	if (route & 0x100) {
		printf("l3 s1 ");
	}

	printf("\n");
}


DWORD K10Processor::getHTLinkDistributionTarget(DWORD link, DWORD *DstLnk,
		DWORD *DstNode) {

	//Coherent Link Traffic Distribution Register:
	PCIRegObject *cltdRegObject;

	//Routing Table register:
	PCIRegObject *routingTableRegObject;
	DWORD routingTableRegister = 0x40;

	int i;

	cltdRegObject = new PCIRegObject();

	if (!cltdRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG,
			0x164, getNodeMask())) {
		printf(
				"K10Processor::getHTLinkDistributionTarget - unable to read Coherent Link Traffic Distribution PCI Register\n");
		free(cltdRegObject);
		return 0;
	}

	//Destination link is set in bits 16-23
	//*DstLnk = (miscReg >> 16) & 0x7F;
	*DstLnk = cltdRegObject->getBits(0, 16, 7);

	//Destination node is set in bits 8-11
	//*DstNode = (miscReg >> 8) & 0x7;
	*DstNode = cltdRegObject->getBits(0, 8, 3);

	for (i = 0; i < 8; i++) {
		DWORD BcRoute;
		DWORD RpRoute;
		DWORD RqRoute;

		routingTableRegObject = new PCIRegObject();
		if (!routingTableRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_HT_CONFIG, routingTableRegister, getNodeMask())) {
			printf(
					"K10Processor::getHTLinkDistributionTarget - unable to read Routing Table PCI Register\n");
			free(cltdRegObject);
			free(routingTableRegObject);
			return 0;
		}

		//Broadcast Route is set in bits 18-26
		//BcRoute = (miscReg >> 18) & 0x1ff;
		BcRoute = routingTableRegObject->getBits(0, 18, 9);

		//Response Route is set in bits 9-17
		//RpRoute = (miscReg >> 9) & 0x1ff;
		RpRoute = routingTableRegObject->getBits(0, 9, 9);

		//Request Route is set in bits 0-8
		//RqRoute = miscReg & 0x1ff;
		RqRoute = routingTableRegObject->getBits(0, 0, 9);

		printf("route node=%u\n", i);

		printf("BroadcastRoute = ");
		printRoute(BcRoute);
		printf("ResponseRoute  = ");
		printRoute(RpRoute);
		printf("RequestRoute   = ");
		printRoute(RqRoute);

		routingTableRegister += 0x4;

		free(routingTableRegObject);
	}

	free(cltdRegObject);

	return 0;
}


void K10Processor::setHTLinkSpeed (DWORD linkRegister, DWORD reg) {

	PCIRegObject *pciRegObject;

	if ((reg==1) || (reg==3) || (reg==15) || (reg==16) || (reg<=0) || (reg>=0x14)) {
		printf ("setHTLinkSpeed: invalid HT Link registry value\n");
		return;
	}

	/*
	 * Hypertransport Link Speed Register is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_0
	 * register 0x88 + 0x20 * linkId
	 * bits from 8 to 11
	 *
	 */

	linkRegister=0x88 + 0x20 * linkRegister;

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, linkRegister, getNodeMask())) {
		printf ("K10Processor.cpp::setHTLinkSpeed - unable to read PCI register\n");
		free (pciRegObject);
		return;
	}

	pciRegObject->setBits(8, 4, reg);

	if (!pciRegObject->writePCIReg()) {
		printf ("K10Processor.cpp::setHTLinkSpeed - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;
}

// CPU Usage module


bool K10Processor::getPsiEnabled () {

	PCIRegObject *pciRegObject;
	DWORD psiEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("K10Processor.cpp::getPsiEnabled - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * Psi bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xa0
	 * bit 7
	 */

	psiEnabled=pciRegObject->getBits(0, 7, 1);

	free(pciRegObject);

	return (bool) psiEnabled;

}

DWORD K10Processor::getPsiThreshold () {

	PCIRegObject *pciRegObject;
	DWORD psiThreshold;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("K10Processor.cpp::getPsiThreshold - unable to read PCI register\n");
		free(pciRegObject);
		return false;
	}

	/*
	 * Psi threshold is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xa0
	 * bits from 0 to 6
	 */

	psiThreshold=pciRegObject->getBits(0, 0, 7);

	free(pciRegObject);

	return psiThreshold;

}

void K10Processor::setPsiEnabled (bool toggle) {

	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("K10Processor.cpp::setPsiEnabled - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * Psi bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xa0
	 * bit 7
	 */

	pciRegObject->setBits(7, 1, toggle);

	if (!pciRegObject->writePCIReg()) {
		printf ("K10Processor.cpp::setPsiEnabled - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
	
}

void K10Processor::setPsiThreshold (DWORD threshold) {

	PCIRegObject *pciRegObject;

	if (threshold>minVID() || threshold<maxVID()) {
			printf ("setPsiThreshold: value must be between %d and %d\n",minVID(), maxVID());
			return;
		}


	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("K10Processor.cpp::setPsiThreshold - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * Psi threshold is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xa0
	 * bits from 0 to 6
	 */

	pciRegObject->setBits(0, 7, threshold);

	if (!pciRegObject->writePCIReg()) {
		printf ("K10Processor.cpp::setPsiThreshold - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

// Various settings

bool K10Processor::getC1EStatus () {

	MSRObject *msrObject;
	DWORD c1eBit;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(CMPHALT_REG, getMask())) {
		printf ("K10Processor.cpp::getC1EStatus - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//C1E bit is stored in bit 28
	c1eBit=msrObject->getBitsLow(0, 28, 1);

	free (msrObject);

	return (bool) c1eBit;
	
}

void K10Processor::setC1EStatus (bool toggle) {

	MSRObject *msrObject;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(CMPHALT_REG, getMask())) {
		printf ("K10Processor.cpp::setC1EStatus - unable to read MSR\n");
		free (msrObject);
		return;
	}

	msrObject->setBitsLow(28, 1, toggle);

	//C1E bit is stored in bit 28
	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp::setC1EStatus - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

// Performance Counters

/*
 * Will show some informations about performance counter slots
 */
void K10Processor::perfCounterGetInfo () {

	K10Processor::K10PerformanceCounters::perfCounterGetInfo(this);

}

/*
 * perfCounterGetValue will retrieve and show the performance counter value for all the selected nodes/processors
 *
 */
void K10Processor::perfCounterGetValue (unsigned int perfCounter) {

	PerformanceCounter *performanceCounter;

	performanceCounter=new PerformanceCounter(getMask(), perfCounter, this->getMaxSlots());

	if (!performanceCounter->takeSnapshot()) {
		printf ("K10PerformanceCounters::perfCounterGetValue - unable to read performance counter");
		free (performanceCounter);
		return;
	}

	printf ("Performance counter value: (decimal)%ld (hex)%lx\n", performanceCounter->getCounter(0), performanceCounter->getCounter(0));

}

void K10Processor::perfMonitorCPUUsage () {

	K10Processor::K10PerformanceCounters::perfMonitorCPUUsage(this);

}

void K10Processor::perfMonitorFPUUsage () {

	K10Processor::K10PerformanceCounters::perfMonitorFPUUsage(this);

}

void K10Processor::perfMonitorDCMA () {

	K10Processor::K10PerformanceCounters::perfMonitorDCMA(this);

}


void K10Processor::getCurrentStatus (struct procStatus *pStatus, DWORD core) {

    DWORD eaxMsr, edxMsr;

    RdmsrPx (0xc0010071,&eaxMsr,&edxMsr,(DWORD_PTR)1<<core);
    pStatus->pstate=(eaxMsr>>16) & 0x7;
    pStatus->vid=(eaxMsr>>9) & 0x7f;
    pStatus->fid=eaxMsr & 0x3f;
    pStatus->did=(eaxMsr >> 6) & 0x7;

return;

}

void K10Processor::checkMode () {

	DWORD a, b, c, i, j, k, pstate, vid, fid, did;
	DWORD eaxMsr, edxMsr;
	DWORD timestamp;
	DWORD states[processorNodes][processorCores][getPowerStates()];
	DWORD savedstates[processorNodes][processorCores][getPowerStates()];
	DWORD minTemp, maxTemp, temp, savedMinTemp, savedMaxTemp;
	DWORD oTimeStamp, iTimeStamp;
	float curVcore;
	DWORD maxPState;
	unsigned int cid;

	maxPState=getMaximumPState().getPState();

	for (i = 0; i < processorNodes; i++)
	{
		for (j = 0; j < processorCores; j++)
		{
			for (k = 0; k < getPowerStates(); k++)
			{
				states[i][j][k] = 0;
				savedstates[i][j][k] = 0;
			}
		}
	}

	minTemp=getTctlRegister();
	maxTemp=minTemp;
	iTimeStamp = GetTickCount();
	oTimeStamp = iTimeStamp;

	while (1) {
	        ClearScreen(CLEARSCREEN_FLAG_SMART);

		timestamp=GetTickCount ();

		printf ("\nTs:%u - ",timestamp);
		for (i = 0; i < processorNodes; i++)
		{
			setNode(i);
			printf("\nNode %d\t", i);
			
			for (j = 0; j < processorCores; j++)
			{
				RdmsrPx (0xc0010071, &eaxMsr, &edxMsr, (PROCESSORMASK)1 << (i * processorCores + j));
				pstate = (eaxMsr >> 16) & 0x7;
				vid = (eaxMsr >> 9) & 0x7f;
				curVcore = (float)((124 - vid) * 0.0125);
				fid = eaxMsr & 0x3f;
				did = (eaxMsr >> 6) & 0x7;

				states[i][j][pstate]++;

				printf ("c%d:ps%d - ", j, pstate);
			}

			temp=getTctlRegister();

			if (temp<minTemp) minTemp=temp;
			if (temp>maxTemp) maxTemp=temp;

			printf ("Tctl: %d",temp);
		}

		if ((timestamp-oTimeStamp)>30000) {
			oTimeStamp=timestamp;

			for (a = 0; a < processorNodes; a++)
			{
				for (b = 0; b < processorCores; b++)
				{
					for (c = 0; c < getPowerStates(); c++)
					{
						savedstates[a][b][c] = states[a][b][c];
						states[a][b][c] = 0;
					}
				}
			}
			savedMinTemp = minTemp;
			savedMaxTemp = maxTemp;
			minTemp = getTctlRegister();
			maxTemp = minTemp;
		}
		fflush(stdout);

		if ((timestamp - iTimeStamp) > 30000)
		{
			for (a = 0; a < processorNodes; a++)
			{
				printf("\nNode%d", a);
				for (b = 0; b < processorCores; b++)
				{
				        if ((b & 1) == 0)
				                printf("\n");
                                        else
                                                printf("      ");
					printf(" C%d:", b);
					for (c = 0; c < getPowerStates(); c++)
					{
						printf("%6d", savedstates[a][b][c]);
					}
				}
			}
			printf ("\nMinTctl:%d\t MaxTctl:%d\n\n", savedMinTemp, savedMaxTemp);
		}
		fflush(stdout);


		Sleep (50);
	}

	return;
}


/***************** PRIVATE METHODS ********************/


/*
 * dram bank is valid
 */
bool K10Processor::getDramValid (DWORD device) {

	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();

	bool reg1;

	dramConfigurationHighRegister=new PCIRegObject ();

	if (device==0) {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());
	} else {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());
	}

	if (!reg1) {
		printf("K10Processor::getDramValid - unable to read PCI registers\n");
		free(dramConfigurationHighRegister);
		return false;
	}

	return dramConfigurationHighRegister->getBits(0,3,1);

}

/*
 * Determines if DCT is in DDR3 mode. True means DDR3, false means DDR2
 */
bool K10Processor::getDDR3Mode (DWORD device) {

	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();

	bool reg1;

	dramConfigurationHighRegister=new PCIRegObject ();

	if (device==0) {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());
	} else {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());
	}

	if (!reg1) {
		printf("K10Processor::getDDR3Mode - unable to read PCI registers\n");
		free(dramConfigurationHighRegister);
		return false;
	}

	return dramConfigurationHighRegister->getBits(0,8,1);

}

int K10Processor::getDramFrequency (DWORD device) {

	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();

	bool reg1;

	DWORD regValue;

	dramConfigurationHighRegister=new PCIRegObject ();

	if (device==0) {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());
	} else {
		reg1=dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
			PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());
	}

	if (!reg1) {
		printf("K10Processor::getDRAMFrequency - unable to read PCI registers\n");
		free(dramConfigurationHighRegister);
		return false;
	}

	regValue=dramConfigurationHighRegister->getBits(0,0,3);

	switch (getDDR3Mode(device)) {
		case true:
			return (int)(400 + (regValue-3)*(float)133.4);
		case false:
			if (regValue==0x4) regValue++; //in case of regvalue==100b (4), increments regvalue by one to match 533 Mhz
			return (int)(200 + (regValue)*(float)66.7);
		default:
			return 0;
	}

	return 0;

}

void K10Processor::getDramTimingHigh(DWORD device, DWORD *TrwtWB,
		DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwr, DWORD *Trdrd,
		DWORD *Tref, DWORD *Trfc0, DWORD *Trfc1, DWORD *Trfc2, DWORD *Trfc3,
		DWORD *MaxRdLatency) {

	PCIRegObject *dramTimingHighRegister = new PCIRegObject();
	PCIRegObject *dramControlRegister = new PCIRegObject();

	bool reg1;
	bool reg2;

	if (device == 1) {
		reg1 = dramTimingHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x18c, getNodeMask());
		reg2 = dramControlRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x178, getNodeMask());
	} else {
		reg1 = dramTimingHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x8c, getNodeMask());
		reg2 = dramControlRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x78, getNodeMask());
	}

	if (!(reg1 && reg2)) {
		printf(
				"K10Processor::getDRAMTimingHigh - unable to read PCI registers\n");
		free(dramTimingHighRegister);
		free(dramControlRegister);
		return;
	}

	*MaxRdLatency = dramControlRegister->getBits(0, 22, 10); //(DRAMControlReg >> 22) & 0x3ff;

	//ReadPciConfigDwordEx (Target,DRAMTimingHighRegister,&miscReg);

	*TrwtWB = dramTimingHighRegister->getBits(0, 0, 4); //(miscReg >> 0) & 0x0f;
	*TrwtTO = dramTimingHighRegister->getBits(0, 4, 4); //(miscReg >> 4) & 0x0f;
	*Twtr = dramTimingHighRegister->getBits(0, 8, 2); //(miscReg >> 8) & 0x03; TODO: fix this in case of DDR3!
	*Twrrd = dramTimingHighRegister->getBits(0, 10, 2); //(miscReg >> 10) & 0x03;
	*Twrwr = dramTimingHighRegister->getBits(0, 12, 2); //(miscReg >> 12) & 0x03;
	*Trdrd = dramTimingHighRegister->getBits(0, 14, 2); //(miscReg >> 14) & 0x03;
	*Tref = dramTimingHighRegister->getBits(0, 16, 2); //(miscReg >> 16) & 0x03;
	*Trfc0 = dramTimingHighRegister->getBits(0, 20, 3); //(miscReg >> 20) & 0x07;
	*Trfc1 = dramTimingHighRegister->getBits(0, 23, 3); //(miscReg >> 23) & 0x07;
	*Trfc2 = dramTimingHighRegister->getBits(0, 26, 3); //(miscReg >> 26) & 0x07;
	*Trfc3 = dramTimingHighRegister->getBits(0, 29, 3); //(miscReg >> 29) & 0x07;

	if (getDDR3Mode(device) || getDramFrequency(device) == 533) {

		//Adjusting timings for DDR3/DDR2-1066 memories.

		*TrwtWB += 3;
		*TrwtTO += 2;
		*Twtr += 4;

		if (!getDDR3Mode(device)) {
			*Twrrd += 1;
			*Twrwr += 1;
			*Trdrd += 2;
		} else {
			*Twrrd += (dramControlRegister->getBits(0,8,2)<<2) + 0;
			*Twrwr += (dramControlRegister->getBits(0,10,2)<<2) + 1;
			*Trdrd += (dramControlRegister->getBits(0,12,2)<<2) + 2;
		}

	} else {

		//Adjusting timings for DDR2 memories

		*TrwtWB += 0;
		*TrwtTO += 2;
		*Twtr += 0;
		*Twrrd += 1;
		*Twrwr += 1;
		*Trdrd += 2;

	}

	free(dramTimingHighRegister);
	free(dramControlRegister);

	return;
}


void K10Processor::getDramTimingLow(
		DWORD device, // 0 or 1   DCT0 or DCT1
		DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras,
		DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *Tcwl, DWORD *T_mode,
		DWORD *Tfaw) {

	bool reg1;
	bool reg2;
	bool reg3;

	PCIRegObject *dramTimingLowRegister = new PCIRegObject();
	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();
	PCIRegObject *dramMsrRegister = new PCIRegObject();

	if (device == 1) {

		reg1 = dramMsrRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x184, getNodeMask());
		reg2 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x188, getNodeMask());
		reg3 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());

	} else {

		reg1 = dramMsrRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x84, getNodeMask());
		reg2 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x88, getNodeMask());
		reg3 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());

	}

	if (!(reg1 && reg2 && reg3)) {
		printf("K10Processor::getDRAMTimingLow - unable to read PCI register\n");
		free(dramMsrRegister);
		free(dramTimingLowRegister);
		free(dramConfigurationHighRegister);
		return;
	}

	//ReadPciConfigDwordEx (Target,DRAMConfigurationHighRegister,&miscReg);

	if (dramConfigurationHighRegister->getBits(0, 20, 1)) {
		*T_mode = 2;
	} else {
		*T_mode = 1;
	}

	// 0 = no tFAW window restriction
	// 1b= 16 memclk .... 1001b= 32 memclk
	*Tfaw = dramConfigurationHighRegister->getBits(0, 28, 4) << 1; //((miscReg >> 28) << 1);
	if (*Tfaw != 0) {
		if (getDDR3Mode(device) || getDramFrequency(device) == 533)
			*Tfaw += 14;
		else
			*Tfaw += 7;
	}

	if (dramConfigurationHighRegister->getBits(0, 14, 1)) {
		printf("interface disabled on node %u DCT %u\n", selectedNode, device);
		return;
	}

	*Tcl = dramTimingLowRegister->getBits(0, 0, 4); //(miscReg) & 0x0F;
	*Trcd = dramTimingLowRegister->getBits(0, 4, 3); //(miscReg >> 4) & 0x07;
	*Trp = dramTimingLowRegister->getBits(0, 7, 3); //(miscReg >> 7) & 0x07;
	*Trtp = dramTimingLowRegister->getBits(0, 10, 2); //(miscReg >> 10) & 0x03;
	*Tras = dramTimingLowRegister->getBits(0, 12, 4); //(miscReg >> 12) & 0x0F;
	*Trc = dramTimingLowRegister->getBits(0, 16, 5); //(miscReg >> 16) & 0x1F;	// ddr3 size
	*Trrd = dramTimingLowRegister->getBits(0, 22, 2); //(miscReg >> 22) & 0x03;
	*Twr = dramMsrRegister->getBits(0, 4, 3); //(miscReg >> 4) & 0x07; // assumes ddr3
	*Tcwl = dramMsrRegister->getBits(0, 20, 3); //(miscReg >> 20) & 0x07;

	if (getDDR3Mode(device) || getDramFrequency(device) == 533) {

		//Assumes DDR3/DDR2-1066 memory type

		if (!getDDR3Mode(device))
			*Tcl += 1;
		else
			*Tcl += 4;

		*Trcd += 5;
		*Trp += 5;

		if (!getDDR3Mode(device)) {
			*Trtp = dramTimingLowRegister->getBits(0, 11, 1);
			*Trtp += 2;
		} else
			*Trtp += 4;

		*Tras += 15;
		*Trc += 11;

		*Twr += 4;
		if (*Twr > 8) {
			*Twr += *Twr - 8;
		}

		*Trrd += 4;
		*Tcwl += 5;

	} else {

		*Tcl += 1;

		//For Trcd, in case of DDR2 first MSB looks like must be discarded
		*Trcd = dramTimingLowRegister->getBits(0, 4, 2);
		*Trcd += 3;

		//For Trp, in case of DDR2 first LSB looks like must be discarded
		*Trp = dramTimingLowRegister->getBits(0, 6, 2);
		*Trp += 3;

		//For Trtp, in case of DDR2 first LSB looks like must be discarded
		*Trtp = dramTimingLowRegister->getBits(0, 11, 1); //(miscReg >> 10) & 0x03;
		*Trtp += 2;

		*Tras += 3;

		*Trc = dramTimingLowRegister->getBits(0, 16, 3); //(miscReg >> 16) & 0x1F;	// ddr2 < 1066 size
		*Trc += 11;

		*Trrd = dramTimingLowRegister->getBits(0, 22, 2); //(miscReg >> 22) & 0x03;
		*Trrd += 2;

		*Twr = dramTimingLowRegister->getBits(0, 20, 2); // assumes ddr2
		*Twr += 3;


	}

	free(dramMsrRegister);
	free(dramTimingLowRegister);
	free(dramConfigurationHighRegister);

	return;
}


/************** PUBLIC SHOW METHODS ******************/


void K10Processor::showHTLink() {

	int nodes = getProcessorNodes();
	int i;

	printf("\nHypertransport Status:\n");

	for (i = 0; i < nodes; i++) {

		setNode(i);
		//DWORD DstLnk, DstNode;
		int linknumber;

		for (linknumber = 0; linknumber < 4; linknumber++) {

			int HTLinkSpeed;
			DWORD WidthIn;
			DWORD WidthOut;
			bool fCoherent;
			bool fUnganged;
			DWORD Sublink = 0;

			getHTLinkWidth(linknumber, Sublink, &WidthIn, &WidthOut,
					&fCoherent, &fUnganged);

			if (WidthIn == 0 || WidthOut == 0) {
				printf("Node %u Link %u Sublink %u not connected\n", i,
						linknumber, Sublink);

				continue;
			}

			HTLinkSpeed = getHTLinkSpeed(linknumber, Sublink);

			printf(
					"Node %u Link %u Sublink %u Bits=%u Coh=%u SpeedReg=%d (%dMHz)\n",
					i, linknumber, Sublink, WidthIn, fCoherent,
					//DstLnk,
					//DstNode,
					HTLinkSpeed, HTLinkToFreq(HTLinkSpeed));

			//
			// no sublinks.
			//

			if (!fUnganged) {
				continue;
			}

			Sublink = 1;

			getHTLinkWidth(linknumber, Sublink, &WidthIn, &WidthOut,
					&fCoherent, &fUnganged);

			if (WidthIn == 0 || WidthOut == 0) {
				printf("Node %u Link %u Sublink %u not connected\n", i,
						linknumber, Sublink);

				continue;
			}

			HTLinkSpeed = getHTLinkSpeed(linknumber, Sublink);
			printf(
					"Node %u Link %u Sublink %u Bits=%u Coh=%u SpeedReg=%d (%dMHz)\n",
					i, linknumber, Sublink, WidthIn, fCoherent,
					//DstLnk,
					//DstNode,
					HTLinkSpeed, HTLinkToFreq(HTLinkSpeed));

		}

		// p->getHTLinkDistributionTargetByNode(i, 0, &DstLnk, &DstNode);

		printf("\n");
	}

}

void K10Processor::showHTC() {

	int i;
	int nodes = getProcessorNodes();

	printf("\nHardware Thermal Control Status:\n\n");

	if (HTCisCapable() != true) {
		printf("Processor is not HTC Capable\n");
		return;
	}

	for (i = 0; i < nodes; i++) {
		printf (" --- Node %u:\n", i);
		setNode(i);
		printf("HTC features enabled flag: ");
		if (HTCisEnabled() == true)
			printf("true. Hardware Thermal Control is enabled.\n");
		else
			printf("false. Hardware Thermal Control is disabled.\n");

		printf("HTC features currently active (means overheating): ");
		if (HTCisActive() == true)
			printf("true\n");
		else
			printf("false\n");

		printf("HTC features has been active (means overheated in past): ");
		if (HTChasBeenActive() == true)
			printf("true\n");
		else
			printf("false\n");

		printf("HTC parameters are locked: ");
		if (HTCLocked() == true)
			printf("true\n");
		else
			printf("false\n");

		printf("HTC Slew control: ");
		if (HTCSlewControl() == true)
			printf("by Tctl Slew register\n");
		else
			printf("by Tctl without Slew register\n");

		printf("HTC Limit temperature (equal or above means overheating): %d\n",
			HTCTempLimit());
		printf(
			"HTC Hysteresis temperature (equal or below means no more overheating) : %d\n",
			HTCTempLimit() - HTCHystTemp());
		printf("HTC PState Limit: %d\n", HTCPStateLimit());
		printf("\n");
	}
}

void K10Processor::showDramTimings() {

	int nodes = getProcessorNodes();
	int node_index;
	int dct_index;
	DWORD Tcl, Trcd, Trp, Trtp, Tras, Trc, Twr, Trrd, Tcwl, T_mode;
	DWORD Tfaw, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref, Trfc0;
	DWORD Trfc1, Trfc2, Trfc3, MaxRdLatency;
	bool ddrTypeDDR3;
	DWORD ddrFrequency;

	printf ("\nDRAM Configuration Status\n\n");

	for (node_index = 0; node_index < nodes; node_index++) {

		setNode (node_index);
		printf ("Node %u ---\n", node_index);

		for (dct_index = 0; dct_index < 2; dct_index++) {

			if (getDramValid(dct_index)) {
				int i;

				ddrTypeDDR3=getDDR3Mode (dct_index);
				ddrFrequency=getDramFrequency(dct_index)*2;

				getDramTimingLow(dct_index, &Tcl, &Trcd, &Trp, &Trtp, &Tras, &Trc,
						&Twr, &Trrd, &Tcwl, &T_mode, &Tfaw);

				getDramTimingHigh(dct_index, &TrwtWB, &TrwtTO, &Twtr, &Twrrd,
						&Twrwr, &Trdrd, &Tref, &Trfc0, &Trfc1, &Trfc2, &Trfc3,
						&MaxRdLatency);

				printf("DCT%d: ", dct_index);
				printf ("memory type: ");
				if (ddrTypeDDR3==true) printf ("DDR3"); else printf ("DDR2");
				printf (" frequency: %d MHz\n",ddrFrequency);

				//Low DRAM Register
				printf(
						"Tcl=%u Trcd=%u Trp=%u Tras=%u Access Mode:%uT Trtp=%u Trc=%u Twr=%u Trrd=%u Tcwl=%u Tfaw=%u\n",
						Tcl, Trcd, Trp, Tras, T_mode, Trtp, Trc, Twr, Trrd, Tcwl,
						Tfaw);

				//High DRAM Register
				printf(
						"TrwtWB=%u TrwtTO=%u Twtr=%u Twrrd=%u Twrwr=%u Trdrd=%u Tref=%u Trfc0=%u Trfc1=%u Trfc2=%u Trfc3=%u MaxRdLatency=%u\n",
						TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref, Trfc0,
						Trfc1, Trfc2, Trfc3, MaxRdLatency);

				for (i = 0; i < 8; i++) {
					unsigned int val;
					PCIRegObject *csbaseaddr = new PCIRegObject();
					csbaseaddr->readPCIReg(PCI_DEV_NORTHBRIDGE,
                                                PCI_FUNC_DRAM_CONTROLLER, 0x100 * dct_index + 0x40 + 4 * i, 1 << node_index);
					val = csbaseaddr->getBits(0, 0, 32);
					if ((i & 1) == 0) {
						printf("LDIMM%d=", i >> 1);
					}
					printf("%s", (val & 4) ? "FAILED" : (val & 1) ? "OK" : "EMPTY");
					if ((i & 1) == 0) {
						printf("/");
					} else {
						printf(" ");
					}
					delete csbaseaddr;
				}
				printf("\n");

			} else {

				printf ("DCT%d: - controller inactive -\n", dct_index);
			}
			printf("\n");
		}

		printf("\n");

	} // while

	return;

}
