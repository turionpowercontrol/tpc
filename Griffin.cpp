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
#include "Griffin.h"
#include "MSRObject.h"
#include "PCIRegObject.h"

//Griffin Class constructor
Griffin::Griffin () {

	DWORD eax,ebx,ecx,edx;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("Griffin::Griffin - Fatal error during querying for Cpuid(0x1) instruction.\n");
		return;
	}

	int familyBase = (eax & 0xf00) >> 8;
	int model = (eax & 0xf0) >> 4;
	int stepping = eax & 0xf;
	int familyExtended = ((eax & 0xff00000) >> 20)+familyBase;
	int modelExtended = ((eax & 0xf0000) >> 12)+model;

	//Check Brand ID and Package type - CPUID Function 8000_0001 reg EBX
	if (Cpuid(0x80000001,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("Griffin::Griffin - Fatal error during querying for Cpuid(0x80000001) instruction.\n");
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

	//Check how many physical cores are present - CPUID Function 8000_0008 reg ECX
	if (Cpuid(0x80000008,&eax,&ebx,&ecx,&edx)!=TRUE) {
		printf ("Griffin::Griffin - Fatal error during querying for Cpuid(0x80000008) instruction.\n");
		return;
	}

	int physicalCores=(ecx & 0xff) + 1;

	//Sets Physical cores number
	setProcessorCores (physicalCores);

	//Sets Physical number of processors
	setProcessorNodes (1);

	//Set Power states number
	setPowerStates (8);

	//Detects a Turion ZM processor
	if ((physicalCores==2) && (string1==0) && (pkgType==0x2)) {
		setProcessorIdentifier (TURION_ULTRA_ZM_FAMILY);
		setProcessorStrId ("Turion Ultra ZM Processor");
	}

	//Detects a Turion RM processor
	if ((physicalCores==2) && (string1==1) && (pkgType==0x2)) {
		setProcessorIdentifier (TURION_X2_RM_FAMILY);
		setProcessorStrId ("Turion X2 RM Processor");
	}

	//Detects a Turion QL processor
	if ((physicalCores==2) && (string1==2) && (pkgType==0x2)) {
		setProcessorIdentifier (ATHLON_X2_QL_FAMILY);
		setProcessorStrId ("Athlon X2 QL Processor");
	}

	//Detects a Sempron SI processor 
	if ((physicalCores==1) && (string1==0) && (pkgType==0x2)) {
		setProcessorIdentifier (SEMPRON_SI_FAMILY);
		setProcessorStrId ("Sempron SI Processor");
	}


}


/*
 * Static methods to allow external Main to detect current configuration status
 * without instantiating an object. This method that detects if the system
 * has a processor supported by this module
*/
bool Griffin::isProcessorSupported () {

	DWORD eax;
	DWORD ebx;
	DWORD ecx;
	DWORD edx;

	//Check base CpuID information
	if (Cpuid(0x0,&eax,&ebx,&ecx,&edx)!=TRUE) return false;
	
	//Checks if eax is 0x1, 0x5 or 0x6. It determines the largest CPUID function available
	//Family 11h returns eax=0x1
	if ((eax!=0x1)) return false;

	//Check for "AuthenticAMD" string
	if ((ebx!=0x68747541) || (ecx!=0x444D4163) || (edx!=0x69746E65)) return false;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE) return false;

	int familyBase = (eax & 0xf00) >> 8;
	//int model = (eax & 0xf0) >> 4;
	//int stepping = eax & 0xf;
	int familyExtended = ((eax & 0xff00000) >> 20)+familyBase;
	//int modelExtended = ((eax & 0xf0000) >> 16)+model;

	//Check how many physical cores are present - CPUID Function 8000_0008 reg ECX
	if (Cpuid(0x80000008,&eax,&ebx,&ecx,&edx)!=TRUE) return false;

	int physicalCores=(ecx & 0xff) + 1;

	//Check Brand ID and Package type - CPUID Function 8000_0001 reg EBX
	if (Cpuid(0x80000001,&eax,&ebx,&ecx,&edx)!=TRUE) return false;

	int brandId=(ebx & 0xffff);
	//int processorModel=(brandId >> 4) & 0x7f;
	int string1=(brandId >> 11) & 0xf;
	//int string2=(brandId & 0xf);
	int pkgType=(ebx >> 28);

	if (familyExtended!=0x11) return false;
	
	//We will say that a processor is supported ONLY and if ONLY all parameters gathered
	//above are precisely as stated below. In any other case, we don't detect a valid processor
	if (familyExtended==0x11) {
		//Detects a Turion ZM processor
		if ((physicalCores==2) && (string1==0) && (pkgType==0x2))
			return true;

		//Detects a Turion RM processor
		if ((physicalCores==2) && (string1==1) && (pkgType==0x2))
			return true;

		//Detects a Turion QL processor
		if ((physicalCores==2) && (string1==2) && (pkgType==0x2))
			return true;

		//Detects a Sempron SI processor
		if ((physicalCores==1) && (string1==0) && (pkgType==0x2))
			return true;
	} 

	return false;

}

//Shows specific informations about current processor family
void Griffin::showFamilySpecs () {

	DWORD clock_ramp_hyst;
	DWORD clock_ramp_hyst_ns;
	DWORD psi_l_enable;
	DWORD psi_thres;
	DWORD vddGanged;
	DWORD pstateId;
	unsigned int i;

	PCIRegObject *pciRegObject;

	//Shows Northbridge VID, SMAF7 and C1E info only if we detect a family 11h processor.
	//family 10h processors have a different approach 
	printf ("Processor Northbridge VID: %d (%.3fv)\n",getNBVid(),convertVIDtoVcore(getNBVid()));
	printf ("\n");

	if (getSMAF7Enabled()==true)
		printf ("SMAF7 is enabled; processor is using ACPI SMAF7 tables\n");
	else
		printf ("SMAF7 is disabled; using LMM Configuration registers for power management\n");

	printf ("DID to apply when in C1E state: %d\n",c1eDID());
	
	printf ("\n");

	for (i=0;i<getProcessorCores();i++) {
		setCore(i);
		if (getC1EStatus()==false)
			printf ("Core %d C1E CMP halt bit is disabled\n", i);
		else
			printf ("Core %d C1E CMP halt bit is enabled\n", i);
	}

	printf ("\nVoltage Regulator Slamming time register: %d\n",getSlamTime());

	printf ("Voltage Regulator AltVID Slamming time register: %d\n",getAltVidSlamTime());

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask (ALL_NODES))) {
		printf ("Unable to read PCI Register\n");
	} else {
		//Shows Dual plane/Triple plane only on family 11h processors
		vddGanged=pciRegObject->getBits(0,30,1);
		if (vddGanged==0)
			printf ("Processor is operating in Triple Plane mode\n");
		else
			printf ("Processor is operating in Dual Plane mode\n");

		pstateId=pciRegObject->getBits(0,16,12);
		printf ("Processor PState Identifier: 0x%x\n", pstateId);
	}

	free (pciRegObject);
		
	psi_l_enable=getPsiEnabled();
	psi_thres=getPsiThreshold();

	printf ("Processor is using Serial VID Interface\n");
		

	if (psi_l_enable)
	{
		printf ("PSI_L bit enabled (improve VRM efficiency in low power)\n");
		printf ("PSI voltage threshold VID: %d (%.3fv)\n", psi_thres, convertVIDtoVcore(psi_thres));
	}
	else
		printf ("PSI_L bit not enabled\n");
		
	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask (ALL_NODES))) {
		printf ("Unable to read PCI Register\n");
	} else {

		//Shows Clock ramp hysteresis only on family 11h processors
		clock_ramp_hyst=pciRegObject->getBits(0, 8, 4);
		clock_ramp_hyst_ns=(clock_ramp_hyst+1)*320;

		printf ("Clock ramp hysteresis register: %d (%d ns)\n", clock_ramp_hyst, clock_ramp_hyst_ns);
	}

	free (pciRegObject);

	//testMSR();

}

//Miscellaneous function for some conversions

float Griffin::convertVIDtoVcore (DWORD curVid) {
	return (float)((124-curVid)*0.0125);
}

DWORD Griffin::convertVcoretoVID (float vcore) {
	DWORD vid;

	vid=round(((1.55-vcore)/0.0125));

	return vid;
}

DWORD Griffin::convertFDtoFreq (DWORD curFid, DWORD curDid) {
	return (100*(curFid+0x8))/(1<<curDid);
}

void Griffin::convertFreqtoFD (DWORD freq, int *oFid, int *oDid) {
	/*Needs to calculate the approximate frequency using FID and DID right
	combinations. Take in account that base frequency is always 200 MHz
	(that is Hypertransport 1x link speed).

	For family 11h processor the right formula is:

		(100*(Fid+8))/(2^Did)

	Inverse formulas are:

		fid = (((2^Did) * freq) / 100) - 8

		did = log2 ((100 * (fid + 8))/f)

	The approach I choose here is to minimize DID and maximize FID,
	with respect to the fact that it can't go over the maximum FID.
	Note: Family 11h processors don't like very much changing FID
	between pstates, and often this results in system freezes.
	I still can't figure the reason.

	As you can see, part of the argument of the log of the DID is obtained
	dividing 100*(fid+8) with required frequency.
	Since our maximum FID determines also a maximum operating frequency
	we can calculate the argument doing MaximumFrequency/WantedFrequency

	this way we get the argument of the logarithm. Now we calculate
	the DID calculating the logarithm and then taking only the integer
	part of the result.

	FID then is calculated accordingly, using the inverse formula.

	*/

	float fid;
	float did;

	float argument;

	if (freq==0) return;

	if (maxCPUFrequency()!=0) {

		//Normally all family 11h processors have locked upper multiplier
		//so we can use the idea above to obtain did and fid
		argument=maxCPUFrequency()/(float)freq;

		did=(int)(log(argument)/log((float)2));

	} else {

		//If we got an unlocked multiplier on a Family 11h processor
		//(which is very very unlikely), we fix did=2 and then calculate
		//a valid fid. This is because we can give lower voltages to
		//higher DIDs.
		did=2;

	}

	fid=(((1<<(int)did)*freq)/100)-8;

	if (fid>31) fid=31;

	//printf ("\n\nFor frequency %d, FID is %f, DID %f\n", freq, fid, did);

	*oDid=(int)did;
	*oFid=(int)fid;

	return;

}

//-----------------------setVID-----------------------------
//Overloads abstract class setVID to allow per-core and per-node personalization
void Griffin::setVID (PState ps, DWORD vid) {

	MSRObject *msrObject;

	if ((vid<maxVID()) || (vid>minVID())) {
		printf ("Griffin.cpp::setVID - VID Allowed range %d-%d\n", maxVID(), minVID());
		return;
	}

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("Griffin.cpp::setVID - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To set VID, base offset is 9 bits and value is 7 bit wide.
	msrObject->setBitsLow(9,7,vid);

	if (!msrObject->writeMSR()) {
		printf ("Griffin.cpp::setVID - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

//-----------------------setFID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void Griffin::setFID (PState ps, DWORD fid) {

	MSRObject *msrObject;

	if ((fid<0) || (fid>31)) {
		printf ("Griffin.cpp::setFID - FID Allowed range 0-31\n");
		return;
	}

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("Griffin.cpp::setFID - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To set FID, base offset is 0 bits and value is 6 bit wide
	msrObject->setBitsLow(0,6,fid);

	if (!msrObject->writeMSR()) {
		printf ("Griffin.cpp::setFID - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
}


//-----------------------setDID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void Griffin::setDID(PState ps, DWORD did) {

	MSRObject *msrObject;

	if ((did < 0) || (did > 4)) {
		printf("Griffin.cpp::setDID - DID Allowed range 0-3\n");
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR + ps.getPState(), getMask())) {
		printf("Griffin.cpp::setDID - unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To set DID, base offset is 6 bits and value is 3 bit wide
	msrObject->setBitsLow(6, 3, did);

	if (!msrObject->writeMSR()) {
		printf("Griffin.cpp::setDID - unable to write MSR\n");
		free(msrObject);
		return;
	}

	free (msrObject);

	return;

}

//-----------------------getVID-----------------------------

DWORD Griffin::getVID (PState ps) {

	MSRObject *msrObject;
	DWORD vid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("Griffin.cpp::getVID - unable to read MSR\n");
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

DWORD Griffin::getFID (PState ps) {

	MSRObject *msrObject;
	DWORD fid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("Griffin.cpp::getFID - unable to read MSR\n");
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

DWORD Griffin::getDID (PState ps) {

	MSRObject *msrObject;
	DWORD did;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask())) {
		printf ("Griffin.cpp::getDID - unable to read MSR\n");
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

void Griffin::setFrequency (PState ps, DWORD freq) {

	int fid, did;

	convertFreqtoFD (freq, &fid, &did);

	setFID (ps, (DWORD)fid);
	setDID (ps, (DWORD)did);

	return;
}

//-----------------------setVCore-----------------------------

void Griffin::setVCore (PState ps, float vcore) {

	DWORD vid;
	
	vid=convertVcoretoVID (vcore);

	//Check if VID is below maxVID value set by the processor.
	//If it is, then there are no chances the processor will accept it and
	//we reply with an error
	if (vid<maxVID()) {
		printf ("Unable to set vcore: %0.3fv (vid %d) exceed maximum allowed vcore (%0.3fv)\n", vcore, vid, convertVIDtoVcore(maxVID()));
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

DWORD Griffin::getFrequency (PState ps) {

	DWORD curFid, curDid;
	DWORD curFreq;

	curFid=getFID (ps);
	curDid=getDID (ps);

	curFreq=convertFDtoFreq(curFid, curDid);

	return curFreq;
}

//-----------------------getVCore-----------------------------

float Griffin::getVCore (PState ps) {

	DWORD curVid;
	float curVcore;

	curVid=getVID (ps);
	
	curVcore=convertVIDtoVcore(curVid);

	return curVcore;
}

void Griffin::testMSR() {

	PCIRegObject *pciRegObject;

	unsigned int temp;

	pciRegObject = new PCIRegObject ();

	if (!pciRegObject->readPCIReg(0x3, 0x18, 0x64, 0x1)) {
		printf ("Unable to read PCIRegister\n");
		free (pciRegObject);
		return;
	}

	printf ("HTC is locked %d\n" , pciRegObject->getBits(0, 31, 1));
	printf ("HTC PState limit %d\n" , pciRegObject->getBits(0, 28, 3));
	printf ("Hysteresis %d\n" , (pciRegObject->getBits(0, 24, 4)>>1));
	printf ("HTC slew %d\n" , pciRegObject->getBits(0, 23, 1));

	temp=52 + (pciRegObject->getBits(0, 16, 7)>>1);
	printf ("Temp limit: %d\n" , temp);

	pciRegObject->setBits(24,4,6);

	pciRegObject->writePCIReg();

	free (pciRegObject);

	return;
}


//PStates enable/disable/peek
void Griffin::pStateDisable (PState ps) {

	MSRObject *msrObject;

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR+ps.getPState(), getMask ())) {
		printf ("Griffin.cpp::pStateDisable - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31,1,0x0);

	if (!msrObject->writeMSR()) {
		printf ("Griffin.cpp::pStateDisable - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

void Griffin::pStateEnable (PState ps) {

	MSRObject *msrObject;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR + ps.getPState(), getMask())) {
		printf("Griffin.cpp::pStateEnable - unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To enable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31, 1, 0x1);

	if (!msrObject->writeMSR()) {
		printf("Griffin.cpp::pStateEnable - unable to write MSR\n");
		free(msrObject);
		return;
	}

	free(msrObject);

	return;

}

bool Griffin::pStateEnabled(PState ps) {

	MSRObject *msrObject;
	unsigned int status;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_ZM_PSTATEMSR + ps.getPState(), getMask())) {
		printf("Griffin.cpp::pStateEnabled - unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//To peek a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	//We consider just the first cpu in cpuMask
	status=msrObject->getBitsHigh(0, 31, 1);

	free(msrObject);

	if (status==0) return false; else return true;

}

void Griffin::setMaximumPState (PState ps) {

	PCIRegObject *pciRegObject;

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask())) {
		printf ("Griffin.cpp::setMaximumPState - unable to read PCI register\n");
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
		printf ("Griffin.cpp::setMaximumPState - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;

}

PState Griffin::getMaximumPState () {

	PCIRegObject *pciRegObject;
	PState pState (0);

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask())) {
		printf ("Griffin.cpp::getMaximumPState - unable to read PCI register\n");
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

void Griffin::setNBVid(DWORD nbvid) {

	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc,
			getNodeMask())) {
		printf("Griffin.cpp::setNBVid - unable to read PCI Register\n");
		free(pciRegObject);
		return;
	}

	/* Northbridge VID is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xdc
	 * bits from 12 to 18
	 */
	pciRegObject->setBits(12, 7, nbvid);

	if (!pciRegObject->writePCIReg()) {
		printf("Griffin.cpp::setNBVid - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free (pciRegObject);
	return;

}

DWORD Griffin::getNBVid(void) {

	PCIRegObject *pciRegObject;
	DWORD nbVid;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xdc, getNodeMask())) {
		printf("Griffin.cpp::getNBVid - unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
	}

	/*
	 * Northbridge VID is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xdc
	 * bits from 12 to 18
	 */
	nbVid = pciRegObject->getBits(0, 12, 7);

	free (pciRegObject);

	return nbVid;

}

void Griffin::forcePState (PState ps) {

	MSRObject *msrObject;

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_PSTATE_CTRL_REG, getMask ())) {
		printf ("Griffin.cpp::forcePState - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To force a pstate, we act on setting the first 3 bits of register. All other bits must be zero
	msrObject->setBitsLow(0,32,0x0);
	msrObject->setBitsHigh(0,32,0x0);
	msrObject->setBitsLow(0,3,ps.getPState());

	if (!msrObject->writeMSR()) {
		printf ("Griffin.cpp::forcePState - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;

}

bool Griffin::getSMAF7Enabled () {

	PCIRegObject *pciRegObject;
	DWORD smaf7;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd4, getNodeMask())) {
		printf("Griffin.cpp::getSMAF7Enabled - unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
	}

	/*
	 * SMAF7 bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bit 18
	 */
	smaf7 = !pciRegObject->getBits(0, 18, 1);

	free(pciRegObject);

	return (bool) smaf7;

}

DWORD Griffin::c1eDID() {

	PCIRegObject *pciRegObject;
	DWORD c1eDid;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x1ec, getNodeMask())) {
		printf("Griffin.cpp::c1eDID - unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
	}

	/*
	 * C1E DID bit is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd4
	 * bits from 16 to 18
	 */
	c1eDid = pciRegObject->getBits(0, 16, 3);

	free(pciRegObject);

	return c1eDid;

}

//Be careful, minVid is supposed to work per-node
DWORD Griffin::minVID() {

	MSRObject *msrObject;
	DWORD minVid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("Griffin.cpp::minVID - unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0).
	//MinVid has base offset at 10 bits of high register (edx) and is 7 bit wide
	minVid = msrObject->getBitsHigh(0, 10, 7);

	free(msrObject);

	return minVid;

}

//MaxVID is supposed to work per-node
DWORD Griffin::maxVID () {

	MSRObject *msrObject;
	DWORD maxVid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("Griffin.cpp::maxVID - unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//MaxVid has base offset at 3 bits of high register (edx) and is 7 bit wide
	maxVid = msrObject->getBitsHigh(0, 3, 7);

	free(msrObject);

	return maxVid;
}

//StartupPstate is supposed to be retrieved per node
DWORD Griffin::startupPState() {

	MSRObject *msrObject;
	DWORD pstate;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("Griffin.cpp::startupPState - unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//StartupPState has base offset at 0 bits of high register (edx) and is 3 bit wide
	pstate = msrObject->getBitsHigh(0, 0, 3);

	free(msrObject);

	return pstate;

}

DWORD Griffin::maxCPUFrequency() {

	MSRObject *msrObject;
	DWORD maxCPUFid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode))) {
		printf("Griffin.cpp::maxCPUFrequency unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//maxCPUFid has base offset at 17 bits of high register (edx) and is 6 bits wide
	maxCPUFid = msrObject->getBitsHigh(0, 17, 6);

	free(msrObject);

	return (maxCPUFid + 8) * 100;

}

//Temperature registers ------------------

DWORD Griffin::getTctlRegister (void) {

	PCIRegObject *pciRegObject;
	DWORD temp;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa4, getNodeMask())) {
		printf("Griffin.cpp::getTctlRegister - unable to read PCI register\n");
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

DWORD Griffin::getTctlMaxDiff (void) {

	PCIRegObject *pciRegObject;
	DWORD maxDiff;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa4, getNodeMask())) {
		printf("Griffin.cpp::getTclMaxDiff - unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
	}

	/*
	 * Tctl Max Diff is stored in PCI register with
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
DWORD Griffin::getSlamTime (void) {

	PCIRegObject *pciRegObject;
	DWORD slamTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd8, getNodeMask())) {
		printf("Griffin.cpp::getSlamTime - unable to read PCI register\n");
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

void Griffin::setSlamTime (DWORD slmTime) {

	PCIRegObject *pciRegObject;

	if (slmTime<0 || slmTime >7) {
		printf ("Invalid Slam Time: must be between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd8,
			getNodeMask())) {
		printf("Griffin.cpp::setSlamTime - unable to read PCI Register\n");
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
		printf("Griffin.cpp::setSlamTime - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free (pciRegObject);
	return;

}

DWORD Griffin::getAltVidSlamTime (void) {

	PCIRegObject *pciRegObject;
	DWORD altVidSlamTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xd8, getNodeMask())) {
		printf("Griffin.cpp::getAltVidSlamTime - unable to read PCI register\n");
		free(pciRegObject);
		return NULL;
	}

	/*
	 * voltage slamtime is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_3
	 * register 0xd8
	 * bits from 4 to 6
	 */
	altVidSlamTime = pciRegObject->getBits(0, 4, 3);

	free(pciRegObject);

	return altVidSlamTime;
}

void Griffin::setAltVidSlamTime (DWORD slmTime) {

	PCIRegObject *pciRegObject;

	if (slmTime < 0 || slmTime > 7) {
		printf("Invalid AltVID Slam Time: must be between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd8,
			getNodeMask())) {
		printf("Griffin.cpp::setAltVidSlamTime - unable to read PCI Register\n");
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
	pciRegObject->setBits(4, 3, slmTime);

	if (!pciRegObject->writePCIReg()) {
		printf("Griffin.cpp::setAltVidSlamTime - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);
	return;

}


/***** Available only on Family 10h processors

//Voltage Ramping time
DWORD Griffin::getStepUpRampTime (void) {
	DWORD miscReg;
	DWORD vsRampTime;

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xd4,&miscReg);

	//miscReg=(miscReg & 0xF0FFFFFF) + (0x3 << 24);

	vsRampTime=(miscReg >> 24) & 0xf;

	//WritePciConfigDwordEx (MISC_CONTROL_3,0xd4,miscReg);

	return vsRampTime;
}

DWORD Griffin::getStepDownRampTime (void) {
	DWORD miscReg;
	DWORD vsRampTime;

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xd4,&miscReg);

	vsRampTime=(miscReg >> 20) & 0xf;

	return vsRampTime;
}

void Griffin::setStepUpRampTime (DWORD rmpTime) {
	DWORD miscReg;

	if (rmpTime<0 || rmpTime>0xf) {
		printf ("Invalid Ramp Time: value must be between 0 and 15\n");
		return;
	}

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xd4,&miscReg);

	miscReg=(miscReg & 0xF0FFFFFF) + (rmpTime << 24);

	WritePciConfigDwordEx (MISC_CONTROL_3,0xd4,miscReg);
}

void Griffin::setStepDownRampTime (DWORD rmpTime) {
	DWORD miscReg;

	if (rmpTime<0 || rmpTime>0xf) {
		printf ("Invalid Ramp Time: value must be between 0 and 15\n");
		return;
	}

	ReadPciConfigDwordEx (MISC_CONTROL_3,0xd4,&miscReg);

	miscReg=(miscReg & 0xFF0FFFFF) + (rmpTime << 20);

	WritePciConfigDwordEx (MISC_CONTROL_3,0xd4,miscReg);
}*/


// AltVID - HTC Thermal features

bool Griffin::HTCisCapable() {

	PCIRegObject *pciRegObject;
	DWORD isCapable;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xe8, getNodeMask())) {
		printf("Griffin.cpp::HTCisCapable - unable to read PCI register\n");
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

bool Griffin::HTCisEnabled () {

	PCIRegObject *pciRegObject;
	DWORD isEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCisEnabled - unable to read PCI register\n");
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

bool Griffin::HTCisActive() {

	PCIRegObject *pciRegObject;
	DWORD isActive;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCisActive - unable to read PCI register\n");
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

bool Griffin::HTChasBeenActive () {

	PCIRegObject *pciRegObject;
	DWORD hasBeenActivated;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTChasBeenActive - unable to read PCI register\n");
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


DWORD Griffin::HTCTempLimit () {

	PCIRegObject *pciRegObject;
	DWORD tempLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCTempLimit - unable to read PCI register\n");
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

	tempLimit = 52 + (pciRegObject->getBits(0, 16, 7)>>1);

	free(pciRegObject);

	return tempLimit;

}

bool Griffin::HTCSlewControl () {

	PCIRegObject *pciRegObject;
	DWORD slewControl;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCSlewControl - unable to read PCI register\n");
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

DWORD Griffin::HTCHystTemp () {
	PCIRegObject *pciRegObject;
	DWORD hystTemp;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCHystTemp - unable to read PCI register\n");
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

	hystTemp = pciRegObject->getBits(0, 24, 4)>>1;

	free(pciRegObject);

	return hystTemp;
}

DWORD Griffin::HTCPStateLimit () {

	PCIRegObject *pciRegObject;
	DWORD pStateLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCPStateLimit - unable to read PCI register\n");
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

bool Griffin::HTCLocked () {
	PCIRegObject *pciRegObject;
	DWORD htcLocked;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCLocked - unable to read PCI register\n");
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

void Griffin::HTCEnable() {

	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCEnable - unable to read PCI register\n");
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
		printf("Griffin.cpp::HTCEnable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void Griffin::HTCDisable () {
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCDisable - unable to read PCI register\n");
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
		printf("Griffin.cpp::HTCDisable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void Griffin::HTCsetTempLimit(DWORD tempLimit) {

	PCIRegObject *pciRegObject;

	if (tempLimit < 52 || tempLimit > 115) {
		printf("HTCsetTempLimit: accepted range between 52 and 115\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCsetTempLimit - unable to read PCI register\n");
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
		printf("Griffin.cpp::HTCsetTempLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

void Griffin::HTCsetHystLimit(DWORD hystLimit) {

	PCIRegObject *pciRegObject;

	if (hystLimit < 0 || hystLimit > 7) {
		printf("HTCsetHystLimit: accepted range between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0x64, getNodeMask())) {
		printf("Griffin.cpp::HTCsetHystLimit - unable to read PCI register\n");
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
		printf("Griffin.cpp::HTCsetHystLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

DWORD Griffin::getAltVID () {

	PCIRegObject *pciRegObject;
	DWORD altVid;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xdc, getNodeMask())) {
		printf("Griffin.cpp::getAltVID - unable to read PCI register\n");
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

	altVid=pciRegObject->getBits(0,0,7);

	free(pciRegObject);

	return altVid;

}

void Griffin::setAltVid (DWORD altVid) {

	PCIRegObject *pciRegObject;

	if ((altVid<maxVID()) || (altVid>minVID())) {
		printf ("setAltVID: VID Allowed range %d-%d\n", maxVID(), minVID());
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xdc, getNodeMask())) {
		printf("Griffin.cpp::setAltVID - unable to read PCI register\n");
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
		printf ("Griffin.cpp::setAltVID - unable to write to PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}
	
// Hypertransport Link
DWORD Griffin::getHTLinkWidth(DWORD link, DWORD Sublink, DWORD *WidthIn,
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
				"Griffin::getHTLinkWidth - unable to read linkType PCI Register\n");
		free(linkTypeRegObject);
		return false;
	}

	linkControlRegObject = new PCIRegObject();
	//Link Control Register is located at 0x84 + 0x20 * link
	if (!linkControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			0x84 + (0x20 * link), getNodeMask())) {
		printf(
				"Griffin::getHTLinkWidth - unable to read linkControl PCI Register\n");
		free(linkTypeRegObject);
		free(linkControlRegObject);
		return false;
	}

	linkExtControlRegObject = new PCIRegObject();
	//Link Control Extended Register is located at 0x170 + 0x04 * link
	if (!linkExtControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			0x170 + (0x04 * link), getNodeMask())) {
		printf(
				"Griffin::getHTLinkWidth - unable to read linkExtendedControl PCI Register\n");
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

DWORD Griffin::getHTLinkSpeed (DWORD link, DWORD Sublink) {

	DWORD FUNC_TARGET;

	PCIRegObject *linkRegisterRegObject = new PCIRegObject();
	PCIRegObject *linkExtRegisterRegObject = new PCIRegObject();

	DWORD linkFrequencyRegister = 0x88;
	DWORD linkFrequencyExtensionRegister = 0x9c;

	DWORD dwReturn;

	if( Sublink == 1 )
		FUNC_TARGET=PCI_FUNC_LINK_CONTROL; //function 4
	else
		FUNC_TARGET=PCI_FUNC_HT_CONFIG; //function 0

	linkFrequencyRegister += (0x20 * link);
	linkFrequencyExtensionRegister += (0x20 * link);

	if (!linkRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE,FUNC_TARGET,linkFrequencyRegister,getNodeMask())) {
		printf ("Griffin::getHTLinkSpeed - unable to read linkRegister PCI Register\n");
		free (linkRegisterRegObject);
		free (linkExtRegisterRegObject);
		return false;
	}

	if (!linkExtRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET,
			linkFrequencyExtensionRegister, getNodeMask())) {
		printf(
				"Griffin::getHTLinkSpeed - unable to read linkExtensionRegister PCI Register\n");
		free(linkRegisterRegObject);
		free(linkExtRegisterRegObject);
		return false;
	}

	dwReturn = linkRegisterRegObject->getBits(0,8,4); //dwReturn = (miscReg >> 8) & 0xF;

	//ReadPciConfigDwordEx (Target,LinkFrequencyExtensionRegister,&miscRegExtended);
	//if(miscRegExtended & 1)
	if (linkExtRegisterRegObject->getBits(0,0,1))
	{
		dwReturn |= 0x10;
	}

	// 88, 9c
	// a8, bc
	// c8, dc
	// e8, fc
	//

	return dwReturn;
}

void Griffin::printRoute(DWORD route) {

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


DWORD Griffin::getHTLinkDistributionTarget(DWORD link, DWORD *DstLnk,
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
				"Griffin::getHTLinkDistributionTarget - unable to read Coherent Link Traffic Distribution PCI Register\n");
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
					"Griffin::getHTLinkDistributionTarget - unable to read Routing Table PCI Register\n");
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

void Griffin::setHTLinkSpeed(DWORD linkRegister, DWORD reg) {

	PCIRegObject *pciRegObject;

	if ((reg == 1) || (reg == 3) || (reg == 15) || (reg <= 0) || (reg >= 16)) {
		printf("setHTLinkSpeed: invalid HT Link registry value\n");
		return;
	}

	/*
	 * HT Link Speed is stored in PCI register with
	 * device PCI_DEV_NORTHBRIDGE
	 * function PC_FUNC_MISC_CONTROL_0
	 * register 0x88
	 * bits from 8 to 11
	 */

	linkRegister=0x88 + 0x20 * linkRegister;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG,
			linkRegister, getNodeMask())) {
		printf("Griffin.cpp::setHTLinkSpeed - unable to read PCI register\n");
		free(pciRegObject);
		return;
	}

	pciRegObject->setBits(8, 4, reg);

	if (!pciRegObject->writePCIReg()) {
		printf("Griffin.cpp::setHTLinkSpeed - unable to write PCI Register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

// CPU Usage module

//private funciton to set a Performance Counter with Idle Counter event
void Griffin::setPCtoIdleCounter (int core, int perfCounter) {
	
	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-1)\n");
		return;
	}

	WrmsrPx (BASE_PESR_REG+perfCounter,IDLE_COUNTER_EAX,IDLE_COUNTER_EDX,(PROCESSORMASK)1<<core);

}

//Initializes CPU Usage counter - see the scalers
//Acceptes a pointer to an array of DWORDs, the array is as long as the number of cores the processor has
//Returns true if there are no slots to put the performance counter, while returns false on success.
bool Griffin::initUsageCounter (DWORD *perfReg) {

	DWORD coreId, perf_reg;
	DWORD enabled, event, usrmode, osmode;
	DWORD eaxMsr, edxMsr;
	
	//Finds an empty Performance Counter slot and puts the correct event in the slot.
	for (coreId=0; coreId<processorCores; coreId++) {
		
		perfReg[coreId]=-1;

		for (perf_reg=0x0;perf_reg<0x4;perf_reg++) {

			RdmsrPx (BASE_PESR_REG+perf_reg,&eaxMsr,&edxMsr,coreId+1);
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
DWORD Griffin::getUsageCounter (DWORD *perfReg, DWORD core, int baseTop) {

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
DWORD Griffin::getUsageCounter (DWORD *perfReg, DWORD core) {

	return getUsageCounter (perfReg, core, 100);

}

bool Griffin::getPsiEnabled () {

	PCIRegObject *pciRegObject;
	DWORD psiEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("Griffin.cpp::getPsiEnabled - unable to read PCI register\n");
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

DWORD Griffin::getPsiThreshold () {

	PCIRegObject *pciRegObject;
	DWORD psiThreshold;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("Griffin.cpp::getPsiThreshold - unable to read PCI register\n");
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

void Griffin::setPsiEnabled (bool toggle) {

	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("Griffin.cpp::setPsiEnabled - unable to read PCI register\n");
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
		printf ("Griffin.cpp::setPsiEnabled - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
	
}

void Griffin::setPsiThreshold (DWORD threshold) {
	
	PCIRegObject *pciRegObject;

	if (threshold>minVID() || threshold<maxVID()) {
			printf ("setPsiThreshold: value must be between %d and %d\n",minVID(), maxVID());
			return;
		}


	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3,
			0xa0, getNodeMask())) {
		printf("Griffin.cpp::setPsiThreshold - unable to read PCI register\n");
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
		printf ("Griffin.cpp::setPsiThreshold - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
	
}

// Various settings

bool Griffin::getC1EStatus () {

	MSRObject *msrObject;
	DWORD c1eBit;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(CMPHALT_REG, getMask())) {
		printf ("Griffin.cpp::getC1EStatus - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//C1E bit is stored in bit 28
	c1eBit=msrObject->getBitsLow(0, 28, 1);

	free (msrObject);

	return (bool) c1eBit;
	
}

void Griffin::setC1EStatus (bool toggle) {

	MSRObject *msrObject;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(CMPHALT_REG, getMask())) {
		printf ("Griffin.cpp::setC1EStatus - unable to read MSR\n");
		free (msrObject);
		return;
	}

	msrObject->setBitsLow(28,1,toggle);

	//C1E bit is stored in bit 28
	if (!msrObject->writeMSR()) {
		printf ("Griffin.cpp::setC1EStatus - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
	
}

// Performance Counters - TODO: must be revised

void Griffin::getCurrentStatus (struct procStatus *pStatus, DWORD core) {

	DWORD eaxMsr, edxMsr;

	RdmsrPx (0xc0010071,&eaxMsr,&edxMsr,1<<core);
	pStatus->pstate=(eaxMsr>>16) & 0x7;
	pStatus->vid=(eaxMsr>>9) & 0x7f;
	pStatus->fid=eaxMsr & 0x3f;
	pStatus->did=(eaxMsr >> 6) & 0x7;

return;

}

void Griffin::perfCounterGetInfo () {
	
	DWORD perf_reg;
	DWORD coreId;
	DWORD eaxMsr,edxMsr;
	int event, enabled, usrmode, osmode;

	for (coreId=0; coreId<processorCores; coreId++) {

		for (perf_reg=0;perf_reg<0x4;perf_reg++) {

			RdmsrPx (BASE_PESR_REG+perf_reg,&eaxMsr,&edxMsr,coreId+1);
			event=eaxMsr & 0xff;
			enabled=(eaxMsr >> 22) & 0x1;
			usrmode=(eaxMsr >> 16) & 0x1;
			osmode=(eaxMsr >>17) & 0x1;
		
			printf ("Core %d - Perf Counter %d: EAX:%x EDX:%x - Evt: 0x%x En: %d U: %d OS: %d\n",coreId,perf_reg,eaxMsr,edxMsr,event,enabled,usrmode,osmode);
		}

	}

}

void Griffin::perfCounterGetValue (int core, int perfCounter) {
	
	DWORD eaxMsr,edxMsr;
	uint64_t counter;

	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-1)\n");
		return;
	}

	RdmsrPx (BASE_PERC_REG+perfCounter,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<core);
	counter=((uint64_t)edxMsr<<32)+eaxMsr;

	printf ("\rEAX:%x EDX:%x - Counter: %llu\n",eaxMsr,edxMsr, counter);

}

void Griffin::perfMonitorCPUUsage () {

	DWORD coreId;
	DWORD *perfReg;

	DWORD cpu_usage;
//	DWORD eaxMsr, edxMsr;

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

void Griffin::perfCounterMonitor (int core, int perfCounter) {
	
	DWORD eaxMsr,edxMsr;
	uint64_t pcounter=0, counter=0, diff=0;
	uint64_t pTsc=0, tsc=1;

	if (perfCounter<0 || perfCounter>3) {
		printf ("Performance counter out of range (0-3)\n");
		return;
	}

	if (core<0 || core>processorCores-1) {
		printf ("Core Id is out of range (0-1)\n");
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

void Griffin::checkMode () {

	DWORD i,pstate,vid,fid,did;
	DWORD eaxMsr,edxMsr;
	DWORD timestamp;
	DWORD states[2][8];
	DWORD minTemp,maxTemp,temp;
	DWORD oTimeStamp;
	float curVcore;
	DWORD curFreq;
	DWORD maxPState;

	printf ("Monitoring...\n");
	
	maxPState=getMaximumPState().getPState();

	for (i=0;i<8;i++) {
		states[0][i]=0;
		states[1][i]=0;
	}

	minTemp=getTctlRegister();
	maxTemp=minTemp;
	oTimeStamp=GetTickCount ();

	while (1) {

		timestamp=GetTickCount ();

		printf (" \rTimestamp: %d - ",timestamp);
		for (i=0;i<processorCores;i++) {
			
			/*RdmsrPx (0xc0010063,&eaxMsr,&edxMsr,i+1);
			pstate=eaxMsr & 0x7;*/

			RdmsrPx (0xc0010071,&eaxMsr,&edxMsr,(PROCESSORMASK)1<<i);
			pstate=(eaxMsr>>16) & 0x7;
			vid=(eaxMsr>>9) & 0x7f;
			curVcore=(float)((124-vid)*0.0125);
			fid=eaxMsr & 0x3f;
			did=(eaxMsr >> 6) & 0x7;
			curFreq=(100*(fid+0x8))/(1<<did);

			states[i][pstate]++;

			printf ("c%d:ps%d vc%.3f fr%d - ",i,pstate,curVcore,curFreq);
			if (pstate>maxPState) 
				printf ("\n * Detected pstate %d on core %d\n",pstate,i);
		}


		temp=getTctlRegister();

		if (temp<minTemp) minTemp=temp;
		if (temp>maxTemp) maxTemp=temp;

		printf ("Tctl: %d",temp);

		if ((timestamp-oTimeStamp)>30000) {
			oTimeStamp=timestamp;
			printf ("\n\t0\t1\t2\t3\t4\t5\t6\t7\n");
			printf ("Core0:");
			for (i=0;i<8;i++)
				printf ("\t%d",states[0][i]);
			printf ("\nCore1:");
			for (i=0;i<8;i++)
				printf ("\t%d",states[1][i]);
			printf ("\n\nCurTctl:%d\t MinTctl:%d\t MaxTctl:%d\n",temp,minTemp,maxTemp);
		}
				

		Sleep (50);
	}

	return;
}

/***************** PRIVATE METHODS *******************/

void Griffin::getDramTimingHigh(DWORD device, DWORD *TrwtWB,
		DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwr, DWORD *Trdrd,
		DWORD *Tref, DWORD *Trfc0, DWORD *Trfc1) {

	PCIRegObject *dramTimingHighRegister = new PCIRegObject();

	bool reg1;

	if (device == 1) {
		reg1 = dramTimingHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x18c, getNodeMask());
	} else {
		reg1 = dramTimingHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x8c, getNodeMask());
	}

	if (!reg1) {
		printf(
				"Griffin.cpp::getDRAMTimingHigh - unable to read PCI registers\n");
		free(dramTimingHighRegister);
		return;
	}

	//TrwtWB differers between K10 and K8L even for DDR2<1066
	*TrwtWB = dramTimingHighRegister->getBits(0, 0, 4); //(miscReg >> 0) & 0x0f;
	*TrwtWB += 0;

	*TrwtTO = dramTimingHighRegister->getBits(0, 4, 4); //(miscReg >> 4) & 0x0f;
	*TrwtTO += 2;

	*Twtr = dramTimingHighRegister->getBits(0, 8, 2); //(miscReg >> 8) & 0x03;
	*Twtr += 0;

	*Twrrd = dramTimingHighRegister->getBits(0, 10, 2); //(miscReg >> 10) & 0x03;
	*Twrrd += 1;

	*Twrwr = dramTimingHighRegister->getBits(0, 12, 2); //(miscReg >> 12) & 0x03;
	*Twrwr += 1;

	*Trdrd = dramTimingHighRegister->getBits(0, 14, 2); //(miscReg >> 14) & 0x03;
	*Trdrd += 2;

	*Tref = dramTimingHighRegister->getBits(0, 16, 2); //(miscReg >> 16) & 0x03;

	*Trfc0 = dramTimingHighRegister->getBits(0, 20, 3); //(miscReg >> 20) & 0x07;
	*Trfc1 = dramTimingHighRegister->getBits(0, 23, 3); //(miscReg >> 23) & 0x07;

	free(dramTimingHighRegister);

	return;
}

void Griffin::getDramTimingLow(
		DWORD device, // 0 or 1   DCT0 or DCT1
		DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras,
		DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *T_mode,
		DWORD *Tfaw) {

	bool reg1;
	bool reg2;

	PCIRegObject *dramTimingLowRegister = new PCIRegObject();
	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();

	if (device == 1) {

		reg1 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x188, getNodeMask());
		reg2 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());

	} else {

		reg1 = dramTimingLowRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x88, getNodeMask());
		reg2 = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE,
				PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());

	}

	if (!(reg1 && reg2)) {
		printf("Griffin.cpp::getDRAMTimingLow - unable to read PCI register\n");
		free(dramTimingLowRegister);
		free(dramConfigurationHighRegister);
		return;
	}

	if (dramConfigurationHighRegister->getBits(0, 20, 1)) {
		*T_mode = 2;
	} else {
		*T_mode = 1;
	}

	// 0 = no tFAW window restriction
	// 1b= 8 memclk ....
	*Tfaw = dramConfigurationHighRegister->getBits(0, 28, 4); //((miscReg >> 28) << 1);
	if (*Tfaw != 0) {
		*Tfaw += 7;
	}

	if (dramConfigurationHighRegister->getBits(0, 14, 1)) {
		printf("interface disabled on node %u DCT %u\n", selectedNode, device);
		return;
	}

	*Tcl = dramTimingLowRegister->getBits(0, 0, 4); //(miscReg) & 0x0F;
	*Tcl += 1;

	//For Trcd, in case of DDR2 first MSB looks like must be discarded
	*Trcd = dramTimingLowRegister->getBits(0, 4, 3) & 0x3; //(miscReg >> 4) & 0x07;
	*Trcd += 3;

	//For Trp, in case of DDR2 first LSB looks like must be discarded
	*Trp = dramTimingLowRegister->getBits(0, 7, 3) >> 1; //(miscReg >> 7) & 0x07;
	*Trp += 3;

	//For Trtp, in case of DDR2 first LSB looks like must be discarded
	*Trtp = dramTimingLowRegister->getBits(0, 10, 2)>>1; //(miscReg >> 10) & 0x03;
	*Trtp += 2;

	*Tras = dramTimingLowRegister->getBits(0, 12, 4); //(miscReg >> 12) & 0x0F;
	*Tras += 3;

	*Trc = dramTimingLowRegister->getBits(0, 16, 4); //(miscReg >> 16) & 0x1F;	// ddr2 < 1066 size
	*Trc += 11;

	*Trrd = dramTimingLowRegister->getBits(0, 22, 2); //(miscReg >> 22) & 0x03;
	*Trrd += 2;

	*Twr = dramTimingLowRegister->getBits(0, 20, 2); // assumes ddr2
	*Twr += 4;

	free(dramTimingLowRegister);
	free(dramConfigurationHighRegister);

	return;
}




/***************** PUBLIC SHOW METHODS ***************/


void Griffin::showHTLink() {

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

void Griffin::showHTC() {

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

void Griffin::showDramTimings() {

	int nodes = getProcessorNodes();
	int node_index;
	int dct_index;
	DWORD Tcl, Trcd, Trp, Trtp, Tras, Trc, Twr, Trrd, T_mode;
	DWORD Tfaw, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref, Trfc0;
	DWORD Trfc1;

	printf ("\nDRAM Configuration Status\n\n");

	for (node_index = 0; node_index < nodes; node_index++) {

		setNode (node_index);

		printf ("Node %u ---\n", node_index);

		for (dct_index = 0; dct_index < 2; dct_index++) {

			getDramTimingLow(dct_index, &Tcl, &Trcd, &Trp, &Trtp, &Tras, &Trc,
					&Twr, &Trrd, &T_mode, &Tfaw);

			getDramTimingHigh(dct_index, &TrwtWB, &TrwtTO, &Twtr, &Twrrd,
					&Twrwr, &Trdrd, &Tref, &Trfc0, &Trfc1);

			printf("DCT%d:\n", dct_index);
			//Low DRAM Register
			printf(
					"Tcl=%u Trcd=%u Trp=%u Tras=%u Access Mode:%uT Trtp=%u Trc=%u Twr=%u Trrd=%u Tfaw=%u\n",
					Tcl, Trcd, Trp, Tras, T_mode, Trtp, Trc, Twr, Trrd, Tfaw);

			//High DRAM Register
			printf(
					"TrwtWB=%u TrwtTO=%u Twtr=%u Twrrd=%u Twrwr=%u Trdrd=%u Tref=%u Trfc0=%u Trfc1=%u\n",
					TrwtWB, TrwtTO, Twtr, Twrrd, Twrwr, Trdrd, Tref, Trfc0,
					Trfc1);

		}

		printf("\n");

	} // while

	return;

}
