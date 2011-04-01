#include <stdio.h>
#include <stdlib.h>

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

//K10Processor class constructor
K10Processor::K10Processor () {

	int physicalCores=1;
	DWORD eax,ebx,ecx,edx;
	PCIRegObject *pciReg60;
	PCIRegObject *pciReg160;
	bool pciReg60Success;
	bool pciReg160Success;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("K10Processor::K10Processor - Fatal error during querying for Cpuid(0x1) instruction.\n");
		return;
	}

	int familyBase = (eax & 0xf00) >> 8;
	int model = (eax & 0xf0) >> 4;
	int stepping = eax & 0xf;
	int familyExtended = ((eax & 0xff00000) >> 20)+familyBase;
	int modelExtended = ((eax & 0xf0000) >> 16)+model;

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

    // determine the number of nodes, and number of processors.
	// different operating systems have different APIs for doing similiar, but below steps are OS agnostic.

	pciReg60=new PCIRegObject ();
	pciReg160=new PCIRegObject ();

	pciReg60Success=pciReg60->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_0, 0x60, getNodeMask(1));
	pciReg160Success=pciReg160->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_0, 0x160, getNodeMask (1));


	if (pciReg60Success && pciReg160Success) {

		processorNodes=pciReg60->getBits(0,4,3)+1;

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

	}

	free (pciReg60);
	free (pciReg160);

	//Check how many physical cores are present - CPUID Function 8000_0008 reg ECX
	if (Cpuid(0x80000008, &eax, &ebx, &ecx, &edx) != TRUE) {
		printf(
				"Griffin::Griffin - Fatal error during querying for Cpuid(0x80000008) instruction.\n");
		return;
	}

	physicalCores = (ecx & 0xff) + 1;

	setProcessorCores(physicalCores);
	setPowerStates(5);
	setProcessorIdentifier(PROCESSOR_10H_FAMILY);
	setProcessorStrId("Family 10h Processor");
	setProcessorNodes(1);

	forcePVI = false;
	forceSVI = false;

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
	return PROCESSOR_10H_FAMILY;
}

void K10Processor::showFamilySpecs() {
	DWORD psi_l_enable;
	DWORD psi_thres;
	DWORD pstateId;
	unsigned int i, j;
	PCIRegObject *pciRegObject;

	printf("Northbridge Power States table:\n");

	for (j = 0; j < processorNodes; j++) {
		printf("------ Node %d of %d\n ", j, processorNodes);
		setNode(j);
		setCore(0);
		for (i = 0; i < getPowerStates(); i++) {
			printf("PState %d - NbVid %d (%.4f) NbDid %d NbFid %d\n", i,
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
				PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask(1)))
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
			printf("PSI voltage threshold VID: %d (%.3fv)\n", psi_thres,
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
	 combinations. Take in account that base frequency is always 200 Mhz
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

	if (fid > 31)
		fid = 31;

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
void K10Processor::setFID(PState ps, DWORD fid) {
	MSRObject *msrObject;

	if ((fid < 0) || (fid > 31)) {
		printf("K10Processor.cpp: FID Allowed range 0-31\n");
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
void K10Processor::setDID(PState ps, DWORD did) {
	MSRObject *msrObject;

	if ((did < 0) || (did > 4)) {
		printf("K10Processor.cpp: DID Allowed range 0-3\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
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

DWORD K10Processor::getFID (PState ps) {

	MSRObject *msrObject;
	DWORD fid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("K10Processor.cpp: unable to read MSR\n");
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

DWORD K10Processor::getDID (PState ps) {

	MSRObject *msrObject;
	DWORD did;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//DID is stored after 6 bits of offset and is 3 bits wide
	did=msrObject->getBitsLow(0, 6, 3);

	free (msrObject);

	return did;
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
		printf ("Unable to set vcore: %0.3fv exceed maximum allowed vcore (%0.3fv)\n", vcore, convertVIDtoVcore(maxVID()));
		return;
	}

	//Again we che if VID is above minVID value set by processor.
	if (vid>minVID()) {
		printf ("Unable to set vcore: %0.3fv is below minimum allowed vcore (%0.3fv)\n", vcore, convertVIDtoVcore(minVID()));
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

	if (forcePVI==true) return true;

	if (forceSVI==true) return false;

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask(1))) {
		printf ("Unable to read PCI register (getPVIMode)\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31,1,0x0);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp: unable to write MSR\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31,1,0x1);

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp: unable to write MSR\n");
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
		printf("K10Processor.cpp: unable to read MSR\n");
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
		printf ("K10Processor.cpp: unable to read PCI register\n");
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
		printf ("K10Processor.cpp: unable to write PCI register\n");
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
		printf ("K10Processor.cpp: unable to read PCI register\n");
		free (pciRegObject);
		return NULL;
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

	msrObject=new MSRObject ();

	if ((nbvid<maxVID()) || (nbvid>minVID())) {
		printf ("K10Processor.cpp: Northbridge VID Allowed range 0-127\n");
		return;
	}

	/*
	 * Northbridge VID for a specific pstate must be set for all cores
	 * of a single node. In SVI systems, it should also be coherent with
	 * pstates which have DID=0 or DID=1 (see MSRC001_0064-68 pag. 327 of
	 * AMD Family 10h Processor BKDG document). We don't care here of this
	 * fact, this is a user concern.
	 */

	if (!msrObject->readMSR(BASE_K10_PSTATEMSR+ps.getPState(), getMask(ALL_NODES, selectedNode))) {
		printf ("Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge VID is stored in low half of MSR register (eax) in bits from 25 to 31
	msrObject->setBitsLow(25,7,nbvid);

	if (!msrObject->writeMSR()) {
		printf ("Unable to write MSR\n");
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
		printf ("Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge DID is stored in low half of MSR register (eax) in bit 22
	msrObject->setBitsLow(22,1,nbdid);

	if (!msrObject->writeMSR()) {
		printf ("Unable to write MSR\n");
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
		printf("Unable to read MSR\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To force a pstate, we act on setting the first 3 bits of register. All other bits must be zero
	msrObject->setBitsLow(0,32,0x0);
	msrObject->setBitsHigh(0,32,0x0);
	msrObject->setBitsLow(0,3,ps.getPState());

	if (!msrObject->writeMSR()) {
		printf ("K10Processor.cpp: unable to write MSR\n");
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
		printf("Unable to read MSR\n");
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
		printf("Unable to read MSR\n");
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
		printf ("Unable to read PCI register\n");
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

	if (fid < 0 || fid > 0x1b) {
		printf("setNBFid: fid value must be between 0-27\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd4, getNodeMask())) {
		printf("Unable to read PCI register\n");
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

	pciRegObject->setBits(0, 5, fid);

	if (!pciRegObject->writePCIReg()) {
		printf("Unable to write PCI register\n");
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
		printf ("Unable to read MSR\n");
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
		printf("Unable to read MSR\n");
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
		printf("K10Processor.cpp: unable to read MSR\n");
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
		printf("K10Processor.cpp: unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//maxCPUFid has base offset at 17 bits of high register (edx) and is 6 bits wide
	maxCPUFid = msrObject->getBitsHigh(0, 17, 6);

	free(msrObject);

	return maxCPUFid * 100;

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
			printf("K10Processor.cpp: unable to read PCI register\n");
			free(pciRegObject);
			return NULL;
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
		printf("K10Processor.cpp: unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
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
			0xd4, getNodeMask())) {
		printf("K10Processor.cpp: unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
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

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4,
			getNodeMask())) {
		printf("K10Processor.cpp: unable to read PCI Register\n");
		free(pciRegObject);
		return;
	}

	/*
	 * voltage slamtime is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 0 to 2
	 */

	pciRegObject->setBits(0, 3, slmTime);

	if (!pciRegObject->writePCIReg()) {
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI Register\n");
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
		printf("K10Processor.cpp: unable to read PCI Register\n");
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
			printf("K10Processor.cpp: unable to read PCI Register\n");
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
			printf ("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI Register\n");
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
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to write to PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

// Hypertransport Link

//TODO: All hypertransport Link section requires reworking to use PCIRegObject and MSRObject classes

DWORD K10Processor::getHTLinkSpeed (void) {
	return K10Processor::getHTLinkSpeedByNode( 0, 0, 0 );
}

DWORD K10Processor::getHTLinkWidthByNode (
	DWORD node,
	DWORD link,
	DWORD Sublink,
	DWORD *WidthIn,
	DWORD *WidthOut,
	bool *pfCoherent,
	bool *pfUnganged
	)
{
	DWORD Target;
	DWORD LinkControlRegister = 0x84;
	DWORD LinkExtendedControlRegister = 0x170;
	DWORD LinkTypeRegister = 0x98;
	DWORD miscReg;

	*WidthIn = 0;
	*WidthOut = 0;
	*pfCoherent = FALSE;

	if(Sublink == 1)
	{
		Target = ((0x18+node) << 3) + 4;
	} else {
		Target = ((0x18+node) << 3) + 0;
	}

	LinkTypeRegister += (0x20 *link);

	//
	// determine if the link is connected.
	// TODO check LinkConPend == 0 first.
	//

	if(ReadPciConfigDwordEx (Target,LinkTypeRegister,&miscReg))
	{
		if( ((miscReg >> 2) & 0x1) == 0)
		{
			*pfCoherent = TRUE;
		} else {
			*pfCoherent = FALSE;
		}


		if((miscReg & 1) == 0)
		{
			return 0;
		}
	}


	LinkControlRegister += (0x20 * link);

	if(ReadPciConfigDwordEx (Target,LinkControlRegister,&miscReg))
	{
		int Out = (miscReg >> 28) & 0x7;
		int In = (miscReg >> 24) & 0x7;

		switch (Out)
		{
			case 0:
			{
				*WidthOut = 8;
				break;
			}

			case 1:
			{
				*WidthOut = 16;
				break;
			}

			case 7:
			{
				*WidthOut = 0;
				break;
			}

			default:
			{
				*WidthOut = 0;
				break;
			}
		}

		switch (In)
		{
			case 0:
			{
				*WidthIn = 8;
				break;
			}

			case 1:
			{
				*WidthIn = 16;
				break;
			}

			case 7:
			{
				*WidthIn = 0;
				break;
			}

			default:
			{
				*WidthIn = 0;
				break;
			}
		}
	}

	if( Sublink == 0 )
	{
		LinkExtendedControlRegister += (0x04 * link);

		if(ReadPciConfigDwordEx (Target,LinkExtendedControlRegister,&miscReg))
		{
			if((miscReg & 1) == 0)
			{
				*pfUnganged = TRUE;
			} else {
				*pfUnganged = FALSE;
			}
		}
	}

	return 0;
}

DWORD K10Processor::getHTLinkSpeedByNode (DWORD node, DWORD link, DWORD Sublink) {

	DWORD miscReg;
	DWORD miscRegExtended;

	DWORD Target;

	DWORD LinkFrequencyRegister = 0x88;
	DWORD LinkFrequencyExtensionRegister = 0x9c;

	DWORD dwReturn;

	if( Sublink == 1 )
	{
		Target = ((0x18+node) << 3) + 4;	// function 4.
	} else {
		Target = ((0x18+node) << 3) + 0;
	}

	LinkFrequencyRegister += (0x20 * link);
	LinkFrequencyExtensionRegister += (0x20 * link);


	ReadPciConfigDwordEx (Target,LinkFrequencyRegister,&miscReg);

	dwReturn = (miscReg >> 8) & 0xF;

	ReadPciConfigDwordEx (Target,LinkFrequencyExtensionRegister,&miscRegExtended);
	if(miscRegExtended & 1)
	{
		dwReturn |= 0x10;
	}

	// 88, 9c
	// a8, bc
	// c8, dc
	// e8, fc
	//

#if 0
	ReadPciConfigDwordEx (MISC_CONTROL_0,0x88,&miscReg);

	dwReturn = (miscReg >> 8) & 0xF;

	ReadPciConfigDwordEx (MISC_CONTROL_0,0x9c,&miscRegExtended);
	if(miscRegExtended & 1)
	{
		dwReturn |= 0x10;
	}
#endif

	return dwReturn;
}

void
PrintRoute(
	DWORD route
	)
{

	if( route & 0x1 )
	{
		printf("this ");
	}

	if( route & 0x2 )
	{
		printf("l0 s0 ");
	}

	if( route & 0x4 )
	{
		printf("l1 s0 ");
	}

	if( route & 0x8 )
	{
		printf("l2 s0 ");
	}

	if( route & 0x10 )
	{
		printf("l3 s0 ");
	}

	if(route & 0x20 )
	{
		printf("l0 s1 ");
	}

	if(route & 0x40 )
	{
		printf("l1 s1 ");
	}

	if( route & 0x80 )
	{
		printf("l2 s1 ");
	}

	if( route & 0x100 )
	{
		printf("l3 s1 ");
	}

	printf("\n");
}


DWORD K10Processor::getHTLinkDistributionTargetByNode (
		DWORD node,
		DWORD link,
		DWORD *DstLnk,
		DWORD *DstNode
		)
{
	
	DWORD miscReg;

	DWORD Target;

	DWORD RoutingTableRegister = 0x40;
	DWORD CoherentLinkTrafficDistributionRegister = 0x164;

	int i;

	Target = ((0x18+node) << 3) + 0;

	if(ReadPciConfigDwordEx (Target,CoherentLinkTrafficDistributionRegister,&miscReg))
	{
		*DstLnk = (miscReg >> 16) & 0x7F;
		*DstNode = (miscReg >> 8) & 0x7;
	} else {
		printf("Error reading LinkTrafficDistributionRegister!\n");
	}

	for(i=0;i<8;i++)
	{
		DWORD BcRoute;
		DWORD RpRoute;
		DWORD RqRoute;

		ReadPciConfigDwordEx (Target, RoutingTableRegister,&miscReg);

		BcRoute = (miscReg >> 18) & 0x1ff;
		RpRoute = (miscReg >> 9) & 0x1ff;
		RqRoute = miscReg & 0x1ff;

		printf("route node=%u\n",
			i
			);

		printf("BroadcastRoute = ");
		PrintRoute(BcRoute);
		printf("ResponseRoute  = ");
		PrintRoute(RpRoute);
		printf("RequestRoute   = ");
		PrintRoute(RqRoute);

		RoutingTableRegister += 0x4;
	}

	return 0;
}


void K10Processor::setHTLinkSpeed (DWORD reg) {

	PCIRegObject *pciRegObject;

	if ((reg==1) || (reg==3) || (reg==15) || (reg==16) || (reg<=0) || (reg>=0x14)) {
		printf ("setHTLinkSpeed: invalid HT Link registry value\n");
		return;
	}

#if 0
	ReadPciConfigDwordEx (MISC_CONTROL_0,0x9c,&miscRegExtended);

	if( reg >= 0x10 )
	{
		//if( (miscRegExtended & 1) == 0 )
		//{
			miscRegExtended |= 1;
			WritePciConfigDwordEx (MISC_CONTROL_0,0x9c,miscRegExtended);
		//}
	} else {
		//if(miscRegExtended & 1)
		//{
			miscRegExtended &= !1;
			WritePciConfigDwordEx (MISC_CONTROL_0,0x9c,miscRegExtended);
		//}
	}
#endif

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_0, 0x88, getNodeMask())) {
		printf ("K10Processor.cpp::setHTLinkSpeed - unable to read PCI register\n");
		free (pciRegObject);
		return;
	}

	/*
	 * Hypertransport Link Speed Register is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_0
	 * register 0x88
	 * bits from 8 to 11
	 */

	pciRegObject->setBits(8, 4, reg);

	if (!pciRegObject->writePCIReg()) {
		printf ("K10Processor.cpp::setHTLinkSpeed - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;
}

//TODO: to be fixed and merged with PCIRegObject and MSRObject
void K10Processor::setHTLinkSpeedByNode (
	DWORD node,
	DWORD link,
	DWORD reg
	)
{

	DWORD Target;
	DWORD LinkFrequencyRegister = 0x88;

	DWORD miscReg;

	if ((reg==0x1) || (reg==0x3) || (reg==0xF) || (reg==0x10) || (reg<=0) || (reg>=0x14)) {
		printf ("setHTLinkSpeed: invalid HT Link register value\n");
		return;
	}

	Target = ((0x18+node) << 3) + 0;
	LinkFrequencyRegister += (0x20 * link);

	if(ReadPciConfigDwordEx (Target, LinkFrequencyRegister, &miscReg))
	{

		miscReg=miscReg & 0xFFFFF0FF;
//	miscReg=miscReg + (reg<<8);
		miscReg |= ( (reg & 0xf) << 8 );

		WritePciConfigDwordEx (Target, LinkFrequencyRegister, miscReg);
	}

	printf("ERROR1\n");

	return;
}

// CPU Usage module

//private funciton to set a Performance Counter with Idle Counter event
void K10Processor::setPCtoIdleCounter (int core, int perfCounter) {

	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-%d)\n", processorCores-1);
		return;
	}

	WrmsrPx (BASE_PESR_REG+perfCounter,IDLE_COUNTER_EAX,IDLE_COUNTER_EDX,(PROCESSORMASK)1<<core);

}

//Initializes CPU Usage counter - see the scalers
//Acceptes a pointer to an array of DWORDs, the array is as long as the number of cores the processor has
//Returns true if there are no slots to put the performance counter, while returns false on success.
bool K10Processor::initUsageCounter (DWORD *perfReg) {

	DWORD coreId, perf_reg;
	DWORD enabled, event, usrmode, osmode;
	DWORD eaxMsr, edxMsr;

	//Finds an empty Performance Counter slot and puts the correct event in the slot.
	for (coreId=0; coreId<processorCores; coreId++) {

		perfReg[coreId]=-1;

		for (perf_reg=0x0;perf_reg<0x4;perf_reg++) {

			RdmsrPx (BASE_PESR_REG+perf_reg,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<coreId);
			event=eaxMsr & 0xff;
			enabled=(eaxMsr >> 22) & 0x1;
			usrmode=(eaxMsr >> 16) & 0x1;
			osmode=(eaxMsr >>17) & 0x1;

			//Found an already activated performance slot with right event parameters
			if (event==0x76 && enabled==1 && usrmode==1 && osmode==1) {
				perfReg[coreId]=perf_reg;
				printf ("Core %d is using already set Performace Counter %d\n",coreId,perf_reg);
				break;
			}

			//Found an empty slot ready to be populated
			if (enabled==0) {
				setPCtoIdleCounter (coreId, perf_reg);
				perfReg[coreId]=perf_reg;
				printf ("Core %d is using newly set Performace Counter %d\n",coreId,perf_reg);
				break;
			}
		}

		//If we're unable to set a performance counter for a core, then it is impossible to continue
		//Else do a call to getUsageCounter to initialize its static variables
		if (perfReg[coreId]==-1)
			return true;
		else
			getUsageCounter (perfReg,coreId);
	}

	return false;
}

//Gives CPU Usage in 1/baseTop of the total for specified core
//CPU Usage is equally stretched over any period of time between two calls
//to the function. If baseTop is set to 100, then cpu usage is reported as
//percentage of the total
DWORD K10Processor::getUsageCounter (DWORD *perfReg, DWORD core, int baseTop) {

	static uint64_t tsc[MAX_CORES],pTsc[MAX_CORES],counter[MAX_CORES],pcounter[MAX_CORES];
	DWORD eaxMsr,edxMsr;
	uint64_t diff;

	RdmsrPx (TIME_STAMP_COUNTER_REG,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
	tsc[core]=((uint64_t)edxMsr<<32)+eaxMsr;

	RdmsrPx (BASE_PERC_REG+perfReg[core],&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
	counter[core]=((uint64_t)edxMsr<<32)+eaxMsr;

	diff=((counter[core]-pcounter[core])*baseTop)/(tsc[core]-pTsc[core]);

	pcounter[core]=counter[core];
	pTsc[core]=tsc[core];

	return (DWORD)diff;

}

//Overloads previous method to return a standard percentage of cpu usage.
DWORD K10Processor::getUsageCounter (DWORD *perfReg, DWORD core) {

	return getUsageCounter (perfReg, core, 100);

}

bool K10Processor::getPsiEnabled () {

	PCIRegObject *pciRegObject;
	DWORD psiEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf ("K10Processor.cpp: unable to write PCI register\n");
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
		printf("K10Processor.cpp: unable to read PCI register\n");
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
		printf ("K10Processor.cpp: unable to write PCI register\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
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
		printf ("K10Processor.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//C1E bit is stored in bit 28
	if (!msrObject->setBitsLow(28, 1, toggle)) {
		printf ("K10Processor.cpp: unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
	
}

// Performance Counters

void K10Processor::getCurrentStatus (struct procStatus *pStatus, DWORD core) {

    DWORD eaxMsr, edxMsr;

    RdmsrPx (0xc0010071,&eaxMsr,&edxMsr,(DWORD_PTR)1<<core);
    pStatus->pstate=(eaxMsr>>16) & 0x7;
    pStatus->vid=(eaxMsr>>9) & 0x7f;
    pStatus->fid=eaxMsr & 0x3f;
    pStatus->did=(eaxMsr >> 6) & 0x7;

return;

}

void K10Processor::perfCounterGetInfo () {
	
	DWORD perf_reg;
	DWORD coreId;
	DWORD eaxMsr,edxMsr;
	int event, enabled, usrmode, osmode;

	for (coreId=0; coreId<processorCores; coreId++) {

		for (perf_reg=0;perf_reg<0x4;perf_reg++) {

			RdmsrPx (BASE_PESR_REG+perf_reg,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<coreId);

			event=eaxMsr & 0xff;
			enabled=(eaxMsr >> 22) & 0x1;
			usrmode=(eaxMsr >> 16) & 0x1;
			osmode=(eaxMsr >>17) & 0x1;

			printf ("Core %d - Perf Counter %d: EAX:%x EDX:%x - Evt: 0x%x En: %d U: %d OS: %d\n",coreId,perf_reg,eaxMsr,edxMsr,event,enabled,usrmode,osmode);
		}

	}

}

void K10Processor::perfCounterGetValue (int core, int perfCounter) {

	DWORD eaxMsr,edxMsr;
	uint64_t counter;

	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-%d)\n",processorCores-1);
		return;
	}

	RdmsrPx (BASE_PERC_REG+perfCounter,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
	counter=((uint64_t)edxMsr<<32)+eaxMsr;

	printf ("\rEAX:%x EDX:%x - Counter: %llu\n",eaxMsr,edxMsr, counter);

}

void K10Processor::perfMonitorCPUUsage () {

	DWORD coreId;
	DWORD *perfReg;

	DWORD cpu_usage;

	perfReg=(DWORD*)calloc (processorCores, sizeof (DWORD));

	if (initUsageCounter (perfReg)) {
		printf ("No available performance counter slots. Unable to run CPU Usage Monitoring\n");
		return;
	}

	while (1) {

		printf ("\rCPU Usage:");

		for (coreId=0x0;coreId<processorCores;coreId++) {

			cpu_usage=getUsageCounter (perfReg,coreId);

			printf (" core %d: %d",coreId, cpu_usage);
		}

		Sleep (1000);

		printf ("\n");

	}

	//Never executed, since the always true loop before...
	free (perfReg);
}

void K10Processor::perfCounterMonitor (int core, int perfCounter) {

	DWORD eaxMsr,edxMsr;
	uint64_t pcounter=0, counter=0, diff=0;
	uint64_t pTsc=0, tsc=1;

	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-%d)\n",processorCores-1);
		return;
	}

	printf ("\n");

	while (1) {

		RdmsrPx (TIME_STAMP_COUNTER_REG,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
		tsc=((uint64_t)edxMsr<<32)+eaxMsr;
	
		RdmsrPx (BASE_PERC_REG+perfCounter,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
		counter=((uint64_t)edxMsr<<32)+eaxMsr;

		diff=((counter-pcounter)*100)/(tsc-pTsc);

		printf ("\rEAX:%x EDX:%x - Counter: %llu - Ratio with TSC: %llu\n",eaxMsr,edxMsr, counter, diff);

		pcounter=counter;
		pTsc=tsc;

		Sleep (1000);
	}

}

void K10Processor::forceSVIMode (bool force) {

	printf ("** SVI mode forced\n");

	forceSVI=force;

	if (forcePVI) printf ("** Warning: PVI mode is forced too. May result in undefined behaviour\n");


}

void K10Processor::forcePVIMode (bool force) {

	printf ("** PVI mode forced\n");

	forcePVI=force;

	if (forceSVI) printf ("** Warning: SVI mode is forced too. May result in undefined behaviour\n");

}


void K10Processor::checkMode () {

	DWORD i,pstate,vid,fid,did;
	DWORD eaxMsr,edxMsr;
	DWORD timestamp;
	DWORD states[2][5];
	DWORD minTemp,maxTemp,temp;
	DWORD oTimeStamp;
	float curVcore;
	DWORD maxPState;
	int cid;

	printf ("Monitoring...\n");

	maxPState=getMaximumPState().getPState();

	for (i=0;i<5;i++) {
		states[0][i]=0;
		states[1][i]=0;
	}

	minTemp=getTctlRegister();
	maxTemp=minTemp;
	oTimeStamp=GetTickCount ();

	while (1) {

		timestamp=GetTickCount ();

		printf (" \rTs:%d - ",timestamp);
		for (i=0;i<processorCores;i++) {

			/*RdmsrPx (0xc0010063,&eaxMsr,&edxMsr,i+1);
			pstate=eaxMsr & 0x7;*/

			RdmsrPx (0xc0010071,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<i);
			pstate=(eaxMsr>>16) & 0x7;
			vid=(eaxMsr>>9) & 0x7f;
			curVcore=(float)((124-vid)*0.0125);
			fid=eaxMsr & 0x3f;
			did=(eaxMsr >> 6) & 0x7;

			states[i][pstate]++;

			printf ("c%d:ps%d - ",i,pstate);
			if (pstate>maxPState)
				printf ("\n * Detected pstate %d on core %d\n",pstate,i);
		}

		temp=getTctlRegister();

		if (temp<minTemp) minTemp=temp;
		if (temp>maxTemp) maxTemp=temp;

		printf ("Tctl: %d",temp);

		if ((timestamp-oTimeStamp)>30000) {
			oTimeStamp=timestamp;


			printf ("\n\tps0\tps1\tps2\tps3\tps4\n\n");
			for (cid=0;cid<processorCores;cid++) {
			printf ("Core%d:",cid);
				for (i=0;i<5;i++)
					printf ("\t%d",states[0][i]);

			printf ("\n");
			}

			printf ("\n\nCurTctl:%d\t MinTctl:%d\t MaxTctl:%d\n",temp,minTemp,maxTemp);

		}


		Sleep (50);
	}

	return;
}


/***************** PRIVATE METHODS ********************/


void K10Processor::getDramTimingHigh(DWORD device, DWORD *TrwtWB,
		DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwr, DWORD *Trdrd,
		DWORD *Tref, DWORD *Trfc0, DWORD *Trfc1, DWORD *Trfc2, DWORD *Trfc3,
		DWORD *MaxRdLatency) {

	/*DWORD miscReg;
	 DWORD Target = ((0x18+node) << 3) + 2;

	 DWORD DRAMTimingHighRegister;*/

	PCIRegObject *dramTimingHighRegister = new PCIRegObject();
	PCIRegObject *dramControlRegister = new PCIRegObject();

	bool reg1;
	bool reg2;

	/*if( device == 1 )
	 {
	 DRAMTimingHighRegister = 0x18c;
	 } else {
	 DRAMTimingHighRegister = 0x8c;
	 }*/

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
	*TrwtWB += 3;

	*TrwtTO = dramTimingHighRegister->getBits(0, 4, 4); //(miscReg >> 4) & 0x0f;
	*TrwtTO += 2;

	*Twtr = dramTimingHighRegister->getBits(0, 8, 2); //(miscReg >> 8) & 0x03;
	*Twtr += 4;

	*Twrrd = dramTimingHighRegister->getBits(0, 10, 2); //(miscReg >> 10) & 0x03;
	//	*Twrrd += 0;

	*Twrwr = dramTimingHighRegister->getBits(0, 12, 2); //(miscReg >> 12) & 0x03;
	*Twrwr += 0;

	//TODO: check for bug in specifications, Trdrd should be +2 not +3
	*Trdrd = dramTimingHighRegister->getBits(0, 14, 2); //(miscReg >> 14) & 0x03;
	*Trdrd += 3;

	*Tref = dramTimingHighRegister->getBits(0, 16, 2); //(miscReg >> 16) & 0x03;

	*Trfc0 = dramTimingHighRegister->getBits(0, 20, 3); //(miscReg >> 20) & 0x07;
	*Trfc1 = dramTimingHighRegister->getBits(0, 23, 3); //(miscReg >> 23) & 0x07;
	*Trfc2 = dramTimingHighRegister->getBits(0, 26, 3); //(miscReg >> 26) & 0x07;
	*Trfc3 = dramTimingHighRegister->getBits(0, 29, 3); //(miscReg >> 29) & 0x07;

	free(dramTimingHighRegister);
	free(dramControlRegister);

	return;
}


void K10Processor::getDramTimingLow(
		DWORD device, // 0 or 1   DCT0 or DCT1
		DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras,
		DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *Tcwl, DWORD *T_mode,
		DWORD *Tfaw) {

	/*DWORD miscReg;
	 DWORD Target = ((0x18+node) << 3) + 2;	// F2x[1,0]88 DRAM Timing Low Register*/

	bool reg1;
	bool reg2;
	bool reg3;

	/*DWORD DRAMTimingLowRegister;
	 DWORD DRAMConfigurationHighRegister;
	 DWORD DRAMMRSRegister;*/

	PCIRegObject *dramTimingLowRegister = new PCIRegObject();
	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();
	PCIRegObject *dramMsrRegister = new PCIRegObject();

	/*if( device == 1 )
	 {
	 DRAMMRSRegister = 0x184;
	 DRAMTimingLowRegister = 0x188;
	 DRAMConfigurationHighRegister = 0x194;
	 } else {
	 DRAMMRSRegister = 0x84;
	 DRAMTimingLowRegister = 0x88;
	 DRAMConfigurationHighRegister = 0x94;
	 }*/

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
		*Tfaw += 14;
	}

	if (dramConfigurationHighRegister->getBits(0, 14, 1)) {
		printf("interface disabled on node %u DCT %u\n", selectedNode, device);
		return;
	}

	//ReadPciConfigDwordEx (Target,DRAMTimingLowRegister,&miscReg);

	*Tcl = dramTimingLowRegister->getBits(0, 0, 4); //(miscReg) & 0x0F;
	*Tcl += 4;

	*Trcd = dramTimingLowRegister->getBits(0, 4, 3); //(miscReg >> 4) & 0x07;
	*Trcd += 5;

	*Trp = dramTimingLowRegister->getBits(0, 7, 3); //(miscReg >> 7) & 0x07;
	*Trp += 5;

	*Trtp = dramTimingLowRegister->getBits(0, 10, 2); //(miscReg >> 10) & 0x03;
	*Trtp += 4;

	*Tras = dramTimingLowRegister->getBits(0, 12, 4); //(miscReg >> 12) & 0x0F;
	*Tras += 15;

	*Trc = dramTimingLowRegister->getBits(0, 16, 5); //(miscReg >> 16) & 0x1F;	// ddr3 size
	*Trc += 11;

	//	*Twr = (miscReg >> 20) // bugbug get from reg 0x84
	*Trrd = dramTimingLowRegister->getBits(0, 22, 2); //(miscReg >> 22) & 0x03;
	*Trrd += 4;

	//ReadPciConfigDwordEx (Target,DRAMMRSRegister,&miscReg);

	*Twr = dramMsrRegister->getBits(0, 4, 3); //(miscReg >> 4) & 0x07; // assumes ddr3
	*Twr += 4;

	*Tcwl = dramMsrRegister->getBits(0, 20, 3); //(miscReg >> 20) & 0x07;
	*Tcwl += 5;

	free(dramMsrRegister);
	free(dramTimingLowRegister);
	free(dramConfigurationHighRegister);

	return;
}


/************** PUBLIC SHOW METHODS ******************/


void K10Processor::showHTLink() {

	printf ("\nHypertransport Status:\n");
	printf ("Hypertransport Speed Register: %d (%dMhz)\n",getHTLinkSpeed(),HTLinkToFreq(getHTLinkSpeed()));

}

void K10Processor::showHTC() {

	printf("\nHardware Thermal Control Status:\n");

	if (HTCisCapable() != true) {
		printf("Processor is not HTC Capable\n");
		return;
	}

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
	printf("Processor AltVID: %d (%.3fv)\n", getAltVID(),
			convertVIDtoVcore(getAltVID()));

	return;
}

//TODO: needs to be expanded to deal also with DDR2 memory
void K10Processor::showDramTimings() {

	int nodes = getProcessorNodes();
	int node_index;
	DWORD Tcl, Trcd, Trp, Trtp, Tras, Trc, Twr, Trrd, Tcwl, T_mode;
	DWORD Tfaw, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref, Trfc0;
	DWORD Trfc1, Trfc2, Trfc3, MaxRdLatency;

	for (node_index = 0; node_index < nodes; node_index++) {

		setNode (node_index);

		getDramTimingLow(0, &Tcl, &Trcd, &Trp, &Trtp, &Tras,
				&Trc, &Twr, &Trrd, &Tcwl, &T_mode, &Tfaw);

		getDramTimingHigh(0, &TrwtWB, &TrwtTO, &Twtr, &Twrrd,
				&Twrwr, &Trdrd, &Tref, &Trfc0, &Trfc1, &Trfc2, &Trfc3,
				&MaxRdLatency);

		//Low DRAM Register - DCT 0
		printf(
				"Node %u DCT0: Tcl=%u Trcd=%u Trp=%u Tras=%u Access Mode:%uT Trtp=%u Trc=%u Twr=%u Trrd=%u Tcwl=%u Tfaw=%u\n",
				node_index, Tcl, Trcd, Trp, Tras, T_mode, Trtp, Trc, Twr, Trrd,
				Tcwl, Tfaw);

		//High DRAM Register - DCT 0
		printf(
				"Node %u DCT0: TrwtWB=%u TrwtTO=%u Twtr=%u Twrrd=%u Twrwr=%u Trdrd=%u Tref=%u Trfc0=%u Trfc1=%u Trfc2=%u Trfc3=%u MaxRdLatency=%u\n",
				node_index, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref,
				Trfc0, Trfc1, Trfc2, Trfc3, MaxRdLatency);


		getDramTimingLow(1, &Tcl, &Trcd, &Trp, &Trtp, &Tras,
				&Trc, &Twr, &Trrd, &Tcwl, &T_mode, &Tfaw);

		getDramTimingHigh(1, &TrwtWB, &TrwtTO, &Twtr, &Twrrd,
				&Twrwr, &Trdrd, &Tref, &Trfc0, &Trfc1, &Trfc2, &Trfc3,
				&MaxRdLatency);

		//Low DRAM Register - DCT 1
		printf(
				"Node %u DCT1: Tcl=%u Trcd=%u Trp=%u Tras=%u Access Mode:%uT Trtp=%u Trc=%u Twr=%u Trrd=%u Tcwl=%u Tfaw=%u\n",
				node_index, Tcl, Trcd, Trp, Tras, T_mode, Trtp, Trc, Twr, Trrd,
				Tcwl, Tfaw);

		//High DRAM Register - DCT 1
		printf(
				"Node %u DCT1: TrwtWB=%u TrwtTO=%u Twtr=%u Twrrd=%u Twrwr=%u Trdrd=%u Tref=%u Trfc0=%u Trfc1=%u Trfc2=%u Trfc3=%u MaxRdLatency=%u\n",
				node_index, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref,
				Trfc0, Trfc1, Trfc2, Trfc3, MaxRdLatency);

		printf("\n");

	} // while

	return;

}
