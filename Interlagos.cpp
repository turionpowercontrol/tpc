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
#include "Interlagos.h"
#include "PCIRegObject.h"
#include "MSRObject.h"
#include "PerformanceCounter.h"

#include "sysdep.h"

//Interlagos class constructor
Interlagos::Interlagos ()
{
	DWORD eax,ebx,ecx,edx;
	PCIRegObject *pciReg60;
	PCIRegObject *pciReg160;
	bool pciReg60Success;
	bool pciReg160Success;
	DWORD nodes;
	DWORD cores;
	
	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1,&eax,&ebx,&ecx,&edx)!=TRUE)
	{
		printf ("Interlagos::Interlagos - Fatal error during querying for Cpuid(0x1) instruction.\n");
		return;
	}
	
	int familyBase = (eax & 0xf00) >> 8;
	int model = (eax & 0xf0) >> 4;
	int stepping = eax & 0xf;
	int familyExtended = ((eax & 0xff00000) >> 20) + familyBase;
	int modelExtended = ((eax & 0xf0000) >> 12) + model; /* family 15h: modelExtended is valid */

	//Check Brand ID and Package type - CPUID Function 8000_0001 reg EBX
	if (Cpuid(0x80000001,&eax,&ebx,&ecx,&edx)!=TRUE)
	{
		printf ("Interlagos::Interlagos - Fatal error during querying for Cpuid(0x80000001) instruction.\n");
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
	setMaxSlots (6);

	// determine the number of nodes, and number of processors.
	// different operating systems have different APIs for doing similiar, but below steps are OS agnostic.

	pciReg60 = new PCIRegObject();
	pciReg160 = new PCIRegObject();

	//Are the 0x60 and 0x160 registers valid for Interlagos...they should be, the same board is used for MagnyCours and Interlagos
	pciReg60Success = pciReg60->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, 0x60, getNodeMask(0));
	pciReg160Success = pciReg160->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, 0x160, getNodeMask(0));

	if (pciReg60Success && pciReg160Success)
	{
		nodes = pciReg60->getBits(0, 4, 3) + 1;
	}
	else
	{
		printf ("Warning: unable to detect multiprocessor machine\n");
		nodes = 1;
	}

	free (pciReg60);
	free (pciReg160);

	//Check how many physical cores are present - CPUID Function 8000_0008 reg ECX
	if (Cpuid(0x80000008, &eax, &ebx, &ecx, &edx) != TRUE)
	{
		printf("Interlagos::Interlagos- Fatal error during querying for Cpuid(0x80000008) instruction.\n");
		return;
	}

	cores = ((ecx & 0xff) + 1) / 2; /* cores per node */
	
	/*
	 * Normally we assume that nodes per package is always 1 (one physical processor = one package), but
	 * with Interlagos chips (modelExtended>=8) this is not true since they share a single package for two
	 * chips (16 cores distributed on a single package but on two nodes).
	 */
	int nodes_per_package = 1;
	if (modelExtended >= 8)
	{
		PCIRegObject *pci_F3xE8_NbCapReg = new PCIRegObject();
		
		if ((pci_F3xE8_NbCapReg->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xE8, getNodeMask(0))) == TRUE)
		{
			if (pci_F3xE8_NbCapReg->getBits(0, 29, 1))
			{
				nodes_per_package = 2;
			}
		}
		else
		{
			printf ("Interlagos::Interlagos - Error discovering nodes per package, results may be unreliable\n");
		}

		free(pci_F3xE8_NbCapReg);
	}
	
	setProcessorNodes(nodes);
	setProcessorCores(cores);
	setNode(0);
	setBoostStates (getNumBoostStates());
	setPowerStates(7);
	setProcessorIdentifier(PROCESSOR_15H_FAMILY);
	setProcessorStrId("Family 15h (Bulldozer/Interlagos/Valencia) Processor");
}

/*
 * Static methods to allow external Main to detect current configuration status
 * without instantiating an object. This method that detects if the system
 * has a processor supported by this module
*/
bool Interlagos::isProcessorSupported() {

	DWORD eax;
	DWORD ebx;
	DWORD ecx;
	DWORD edx;

	//Check base CpuID information
	if (Cpuid(0x0, &eax, &ebx, &ecx, &edx) != TRUE)
	  return false;
	
	//Checks if eax is 0xd. It determines the largest CPUID function available
	//Family 15h returns eax=0xd
	if (eax != 0xd)
	  return false;

	//Check "AuthenticAMD" string
	if ((ebx != 0x68747541) || (ecx != 0x444D4163) || (edx != 0x69746E65))
	  return false;

	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if (Cpuid(0x1, &eax, &ebx, &ecx, &edx) != TRUE)
	  return false;

	int familyBase = (eax & 0xf00) >> 8;
	int familyExtended = ((eax & 0xff00000) >> 20) + familyBase;

	if (familyExtended != 0x15)
	  return false;
	
	//Detects a Family 15h processor, i.e. Bulldozer/Interlagos/Valencia
	return true;
}

void Interlagos::showFamilySpecs()
{
	DWORD psi_l_enable;
	DWORD psi_thres;
	DWORD pstateId;
	unsigned int i, j;
	PCIRegObject *pciRegObject;

	printf("Northbridge Power States table:\n");

	for (j = 0; j < processorNodes; j++)
	{
		printf("------ Node %d\n", j);
		setNode(j);
		setCore(0);

		printf("NbVid %d (%0.4fV) NbDid %d NbFid %d NbCOF %d MHz\n", getNBVid(), convertVIDtoVcore(getNBVid()), getNBDid(), getNBFid(), getNBCOF());

		printf("Northbridge Maximum frequency: ");
		if (getMaxNBFrequency() == 0)
		{
			printf("no maximum frequency, unlocked NB multiplier\n");
		}
		else
		{
			printf("%d MHz\n", getMaxNBFrequency());
		}

		if (getPVIMode())
		{
			printf("* Warning: PVI mode is set. Northbridge voltage is used for processor voltage at given pstates!\n");
			printf("* Changing Northbridge voltage changes core voltage too.\n");
		}

		printf("\n");

		for (i = 0; i < getProcessorCores(); i++)
		{
			setCore(i);
			if (getC1EStatus() == false)
			{
				printf("Core %d C1E CMP halt bit is disabled\n", i);
			}
			else
			{
				printf("Core %d C1E CMP halt bit is enabled\n", i);
			}
		}

		printf("\nVoltage Regulator Slamming time register: %d\n", getSlamTime());
		printf("Voltage Regulator Step Up Ramp Time: %d\n", getStepUpRampTime());
		printf("Voltage Regulator Step Down Ramp Time: %d\n", getStepDownRampTime());

		pciRegObject = new PCIRegObject();
		if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
		{
			printf("Unable to read PCI Register (0xa0)\n");
		}
		else
		{
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
		{
			printf("Processor is using Parallel VID Interface (probably Single Plane mode)\n");
		}
		else
		{
			printf("Processor is using Serial VID Interface (probably Dual Plane mode)\n");
		}

		if (psi_l_enable)
		{
			printf("PSI_L bit enabled (improve VRM efficiency in low power)\n");
			printf("PSI voltage threshold VID: %d (%0.4fV)\n", psi_thres, convertVIDtoVcore(psi_thres));
		}
		else
		{
			printf("PSI_L bit not enabled\n");
		}
	}

}

//Miscellaneous function inherited by Processor abstract class and that
//needs to be reworked for family 10h
float Interlagos::convertVIDtoVcore(DWORD curVid)
{

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

	if (getPVIMode())
	{
		if (curVid >= 0x5d)
		{
			curVcore = 0.375;
		}
		else
		{
			if (curVid < 0x3f)
			{
				curVid = (curVid >> 1) << 1;
			}
			curVcore = (float) (1.550 - (0.0125 * curVid));
		}
	}
	else
	{
		if (curVid >= 0x7c)
		{
			curVcore = 0;
		}
		else
		{
			curVcore = (float) (1.550 - (0.0125 * curVid));
		}
	}

	return curVcore;
}

DWORD Interlagos::convertVcoretoVID (float vcore)
{
	DWORD vid;

	vid = round(((1.55 - vcore) / 0.0125));
	
	return vid;

}

DWORD Interlagos::convertFDtoFreq (DWORD curFid, DWORD curDid)
{
	return (100 * (curFid + 0x10)) / (1 << curDid);
}

void Interlagos::convertFreqtoFD(DWORD freq, int *oFid, int *oDid)
{
	/*Needs to calculate the approximate frequency using FID and DID right
	 combinations. Take in account that base frequency is always 200 MHz
	 (that is Hypertransport 1x link speed).

	 For family 15h processor the right formula is:

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
	do
	{

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
void Interlagos::setVID (PState ps, DWORD vid)
{

	MSRObject *msrObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}
	if ((vid > minVID()) || (vid < maxVID()))
	{
		printf ("Interlagos.cpp: VID Allowed range %d-%d\n", minVID(), maxVID());
		return;
	}

	msrObject=new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask ()))
	{
		printf ("Interlagos.cpp: unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To set VID, base offset is 9 bits and value is 7 bit wide.
	msrObject->setBitsLow(9, 7, vid);

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos.cpp: unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
}

//-----------------------setFID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void Interlagos::setFID(PState ps, float floatFid)
{
	unsigned int fid;

	MSRObject *msrObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	fid=(unsigned int)floatFid;

	if (fid > 63)
	{
		printf("Interlagos.cpp: FID Allowed range 0-63\n");
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf("Interlagos.cpp: unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To set FID, base offset is 0 bits and value is 6 bit wide
	msrObject->setBitsLow(0, 6, fid);

	if (!msrObject->writeMSR())
	{
		printf("Interlagos.cpp: unable to write MSR\n");
		free(msrObject);
		return;
	}

	free(msrObject);

	return;
}

//-----------------------setDID-----------------------------
//Overloads abstract Processor method to allow per-core personalization
void Interlagos::setDID(PState ps, float floatDid)
{

	unsigned int did;
	MSRObject *msrObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	did=(unsigned int)floatDid;

	if (did > 4)
	{
		printf("Interlagos.cpp: DID Allowed range 0-4\n");
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf("Interlagos.cpp: unable to read MSR\n");
		free(msrObject);
		return;
	}

	//To set DID, base offset is 6 bits and value is 3 bit wide
	msrObject->setBitsLow(6, 3, did);

	if (!msrObject->writeMSR())
	{
		printf("Interlagos.cpp: unable to write MSR\n");
		free(msrObject);
		return;
	}

	free(msrObject);

	return;

}

//-----------------------getVID-----------------------------
DWORD Interlagos::getVID (PState ps)
{

	MSRObject *msrObject;
	DWORD vid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf ("Interlagos.cpp::getVID - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask.
	//VID is stored after 9 bits of offset and is 7 bits wide
	vid = msrObject->getBitsLow(0, 9, 7);

	free (msrObject);

	return vid;

}

//-----------------------getFID-----------------------------
float Interlagos::getFID (PState ps)
{

	MSRObject *msrObject;
	DWORD fid;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf ("Interlagos.cpp::getFID - unable to read MSR\n");
		free (msrObject);
		return false;
	}

	//Returns data for the first cpu in cpuMask (cpu 0)
	//FID is stored after 0 bits of offset and is 6 bits wide
	fid = msrObject->getBitsLow(0, 0, 6);

	free (msrObject);

	return fid;
}

//-----------------------getDID-----------------------------
float Interlagos::getDID (PState ps)
{

	MSRObject *msrObject;
	DWORD did;

	msrObject=new MSRObject ();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf ("Interlagos.cpp::getDID - unable to read MSR\n");
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
void Interlagos::setFrequency (PState ps, DWORD freq)
{

	int fid, did;

	convertFreqtoFD (freq, &fid, &did);
	
	setFID (ps, (DWORD)fid);
	setDID (ps, (DWORD)did);

	return;
}

//-----------------------setVCore-----------------------------
void Interlagos::setVCore (PState ps, float vcore)
{

	DWORD vid;

	vid=convertVcoretoVID (vcore);

	//Check if VID is below maxVID value set by the processor.
	//If it is, then there are no chances the processor will accept it and
	//we reply with an error
	if (vid<maxVID())
	{
		printf ("Unable to set vcore: %0.4fV exceeds maximum allowed vcore (%0.4fV)\n", vcore, convertVIDtoVcore(maxVID()));
		return;
	}

	//Again we che if VID is above minVID value set by processor.
	if (vid>minVID())
	{
		printf ("Unable to set vcore: %0.4fV is below minimum allowed vcore (%0.4fV)\n", vcore, convertVIDtoVcore(minVID()));
		return;
	}

	setVID (ps,vid);

	return;
}

//-----------------------getFrequency-----------------------------
DWORD Interlagos::getFrequency (PState ps)
{

	DWORD curFid, curDid;
	DWORD curFreq;

	curFid = getFID (ps);
	curDid = getDID (ps);

	curFreq = convertFDtoFreq(curFid, curDid);

	return curFreq;
}

//-----------------------getVCore-----------------------------
float Interlagos::getVCore(PState ps)
{
	DWORD curVid;
	float curVcore;

	curVid = getVID(ps);

	curVcore = convertVIDtoVcore(curVid);

	return curVcore;
}


bool Interlagos::getPVIMode ()
{

	PCIRegObject *pciRegObject;
	bool pviMode;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
	{
		printf ("Interlagos.cpp::getPVIMode - Unable to read PCI register\n");
		return false;
	}

	pviMode = (bool)pciRegObject->getBits(0, 8, 1);

	free (pciRegObject);

	return pviMode;

}

//PStates enable/disable/peek
void Interlagos::pStateDisable (PState ps)
{
	MSRObject *msrObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask ()))
	{
		printf ("Interlagos.cpp::pStateDisable - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31, 1, 0);

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos.cpp::pStateDisable - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
}

void Interlagos::pStateEnable (PState ps)
{
	MSRObject *msrObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask ()))
	{
		printf ("Interlagos.cpp::pStateEnable - unable to read MSR\n");
		free (msrObject);
		return;
	}

	//To disable a pstate, base offset is 63 bits (31th bit of edx) and value is 1 bit wide
	msrObject->setBitsHigh(31, 1, 1);

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos.cpp:pStateEnable - unable to write MSR\n");
		free (msrObject);
		return;
	}

	free (msrObject);

	return;
}

bool Interlagos::pStateEnabled(PState ps)
{
	MSRObject *msrObject;
	unsigned int status;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask()))
	{
		printf("Interlagos.cpp::pStateEnabled - unable to read MSR\n");
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

void Interlagos::setMaximumPState (PState ps)
{
	PCIRegObject *pciRegObject;

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask()))
	{
		printf ("Interlagos.cpp::setMaximumPState - unable to read PCI register\n");
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
	pciRegObject->setBits(8, 3, ps.getPState());

	if (!pciRegObject->writePCIReg())
	{
		printf ("Interlagos.cpp::setMaximumPState - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;
}

PState Interlagos::getMaximumPState ()
{
	PCIRegObject *pciRegObject;
	PState pState (0);

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask()))
	{
		printf ("Interlagos.cpp::getMaximumPState - unable to read PCI register\n");
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

void Interlagos::setNBVid (PState ps, DWORD nbvid)
{
	MSRObject *msrObject;

	msrObject = new MSRObject();

	if ((nbvid < maxVID()) || (nbvid > minVID()))
	{
		printf ("Interlagos.cpp::setNBVid - Northbridge VID Allowed range 0-127\n");
		return;
	}

	/*
	 * Northbridge VID for a specific pstate must be set for all cores
	 * of a single node. In SVI systems, it should also be coherent with
	 * pstates which have DID=0 or DID=1 (see MSRC001_0064-68 pag. 327 of
	 * AMD Family 10h Processor BKDG document). We don't care here of this
	 * fact, this is a user concern.
	 */

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask(ALL_NODES, selectedNode)))
	{
		printf ("Interlagos::setNBVid - Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge VID is stored in low half of MSR register (eax) in bits from 25 to 31
	msrObject->setBitsLow(25, 7, nbvid);

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos::setNBVid - Unable to write MSR\n");
		free (msrObject);
		return;
	}
	
	free (msrObject);

	return;
}

void Interlagos::setNBDid (PState ps, DWORD nbdid)
{
	MSRObject *msrObject;

	msrObject = new MSRObject ();

	if ((nbdid!=0) && (nbdid!=1))
	{
		printf ("Northbridge DID must be 0 or 1\n");
		return;
	}

	if (!msrObject->readMSR(BASE_15H_PSTATEMSR + ps.getPState(), getMask(ALL_CORES, selectedNode)))
	{
		printf ("Interlagos::setNBDid - Unable to read MSR\n");
		free (msrObject);
		return;
	}

	//Northbridge DID is stored in low half of MSR register (eax) in bit 22
	msrObject->setBitsLow(22, 1, nbdid);

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos::setNBDid - Unable to write MSR\n");
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

DWORD Interlagos::getMaxNBFrequency()
{
	MSRObject *msrObject;
	DWORD maxNBFid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode)))
	{
		printf("Interlagos::getMaxNBFrequency - Unable to read MSR\n");
		free(msrObject);
		return false;
	}

	//Maximum Northbridge FID is stored in COFVID_STATUS_REG in higher half
	//of register (edx) in bits from 27 to 31
	maxNBFid = msrObject->getBits(0, 59, 5);

	//If MaxNbFid is equal to 0, then there are no limits on northbridge frequency
	//and so there are no limits on northbridge FID value.
	if (maxNBFid == 0)
		return 0;
	else
		return (maxNBFid * 200);

}

void Interlagos::forcePState (PState ps)
{
	MSRObject *msrObject;
	DWORD boostState = getNumBoostStates();

	if (ps.getPState() >= powerStates) {
		printf("Interlagos.cpp: PState allowed range: 0-%d\n", powerStates - 1);
		return;
	}

	msrObject = new MSRObject();
	
	if (ps.getPState() > 6 - boostState)
	{
		printf ("Interlagos.cpp::forcePState - Forcing PStates on a boosted processor ignores boosted PStates\n");
		printf ("Subtract %d from the PState entered\n", boostState);
		return;
	}

	//Add Boost States as C001_0062 uses software PState Numbering - pg560
	if (!msrObject->readMSR(BASE_PSTATE_CTRL_REG, getMask()))
	{
		printf ("Interlagos.cpp::forcePState - unable to read MSR\n");
		free (msrObject);
		return;
	}
	
	//To force a pstate, we act on setting the first 3 bits of register. All other bits must be zero
	msrObject->setBits(0, 64, 0);
	msrObject->setBitsLow(0, 3, ps.getPState());

	if (!msrObject->writeMSR())
	{
		printf ("Interlagos.cpp::forcePState - unable to write MSR\n");
		free (msrObject);
		return;
	}
	
	printf ("PState set to %d\n", ps.getPState());

	free (msrObject);

	return;
}

DWORD Interlagos::getNBVid()
{
	PCIRegObject *pciRegObject;
	DWORD nbVid;
	bool bnbvid;
	
	pciRegObject = new PCIRegObject();
	
	bnbvid = pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_5, 0x160, getNodeMask());
	
	if (!bnbvid)
	{
		printf("Interlagos::getNBVid - Unable to read MSR\n");
		free (pciRegObject);
		return false;
	}
	
	nbVid = pciRegObject->getBits(0, 10, 7);
	
	free (pciRegObject);
	
	return nbVid;
}

DWORD Interlagos::getNBDid ()
{
	PCIRegObject *pciRegObject;
	DWORD nbDid;
	bool bnbdid;
	
	pciRegObject = new PCIRegObject();
	
	bnbdid = pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_5, 0x160, getNodeMask());
	
	if (!bnbdid)
	{
		printf("Interlagos::getNBDid - Unable to read MSR\n");
		free (pciRegObject);
		return false;
	}
	
	nbDid = pciRegObject->getBits(0, 7, 1);

	free (pciRegObject);

	return nbDid;
}

DWORD Interlagos::getNBFid ()
{
	PCIRegObject *pciRegObject;
	DWORD nbFid;
	bool bnbfid;
	
	pciRegObject = new PCIRegObject();
	
	bnbfid = pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_5, 0x160, getNodeMask());
	
	if (!bnbfid)
	{
		printf ("Interlagos::getNBFid - Unable to read PCI register\n");
		free (pciRegObject);
		return false;
	}

	/*Northbridge FID is stored in
	 *
	 * device PCI_DEV_NORTHBRIDGE
	 * function PCI_FUNC_MISC_CONTROL_5
	 * register 0x160 (PState0)
	 * register 0x164 (PState1)
	 * bits from 1 to 5
	 */

	nbFid = pciRegObject->getBits(0, 1, 5);

	free (pciRegObject);
	
	return nbFid;
}

DWORD Interlagos::getNBCOF()
{
	//Northbridge Current Operating Frequency
	//D18F5x[6C:60][NbFid] + 4 / 2 ^ D18F5x[6C:60][NbDid]
	
	return ((200 * (getNBFid() + 4)) / (1 << getNBDid()));
}

void Interlagos::setNBFid(DWORD fid)
{
	PCIRegObject *pciRegObject;
	unsigned int i, current;
	bool bnbfid;
	
	if (fid > 0x1B)
	{
		printf("setNBFid: fid value must be between 0 and 27\n");
		return;
	}
	
	pciRegObject = new PCIRegObject();
	
	bnbfid = pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_5, 0x160, getNodeMask());
	
	if (!bnbfid)
	{
		printf("Interlagos::setNBFid - Unable to read PCI register\n");
		free(pciRegObject);
		return;
	}
	
	for (i = 0; i < pciRegObject->getCount(); i++)
	{
		current = pciRegObject->getBits(i, 1, 5);
			
		printf("Node %u: current nbfid: %u (%u MHz), target nbfid: %u (%u MHz)\n",
			pciRegObject->indexToAbsolute(i), current, (current + 4) * 200, fid, (fid + 4) * 200);
	}

	pciRegObject->setBits(1, 5, fid);

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::setNBFid - Unable to write PCI register\n");
		free(pciRegObject);
		return;
	}
	
	free(pciRegObject);
	
	return ;
}

//minVID is reported per-node, so selected core is always discarded
DWORD Interlagos::minVID ()
{
	MSRObject *msrObject;
	DWORD minVid;

	msrObject = new MSRObject;

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode)))
	{
		printf ("Interlagos::minVID - Unable to read MSR\n");
		free (msrObject);
		return false;
	}
	
	minVid=msrObject->getBits(0, 42, 7);

	free (msrObject);

	//If minVid==0, then there's no minimum vid.
	//Since the register is 7-bit wide, then 127 is
	//the maximum value allowed.
	if (getPVIMode())
	{
		//Parallel VID mode, allows minimum vcore VID up to 0x5d
		if (minVid == 0)
			return 0x5d;
		else
			return minVid;
	}
	else
	{
		//Serial VID mode, allows minimum vcore VID up to 0x7b
		if (minVid==0)
			return 0x7b;
		else
			return minVid;
	}
}

//maxVID is reported per-node, so selected core is always discarded
DWORD Interlagos::maxVID()
{
	MSRObject *msrObject;
	DWORD maxVid;

	msrObject = new MSRObject;

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode)))
	{
		printf("Interlagos::maxVID - Unable to read MSR\n");
		free(msrObject);
		return false;
	}
	
	maxVid = msrObject->getBits(0, 35, 7);
	
	free(msrObject);
	
	//If maxVid==0, then there's no maximum set in hardware
	if (maxVid == 0)
		return 0;
	else
		return maxVid;
}

//StartupPState is reported per-node. Selected core is discarded
DWORD Interlagos::startupPState ()
{
	MSRObject *msrObject;
	DWORD pstate;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode)))
	{
		printf("Interlagos.cpp::startupPState unable to read MSR\n");
		free(msrObject);
		return false;
	}
	
	pstate = msrObject->getBits(0, 32, 3);
	
	free(msrObject);
	
	return pstate;
}

DWORD Interlagos::maxCPUFrequency()
{
	MSRObject *msrObject;
	DWORD maxCPUFid;

	msrObject = new MSRObject();

	if (!msrObject->readMSR(COFVID_STATUS_REG, getMask(0, selectedNode)))
	{
		printf("Interlagos.cpp::maxCPUFrequency unable to read MSR\n");
		free(msrObject);
		return false;
	}
	
	maxCPUFid = msrObject->getBits(0, 49, 6);
	
	free(msrObject);
	
	return maxCPUFid * 100;
}

/*
 * As per page 416 of BKDG
 * 15C -> Core Performance Boost Control
 * Bits 2:4 to indicate the number of boosted states
 * 
 * If boostlock (bit 31) is not enabled, then it can be modified
 */

DWORD Interlagos::getNumBoostStates(void)
{	
	PCIRegObject *boostControl = new PCIRegObject();
	DWORD numBoostStates;
	
	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("Interlagos::getNumBoostStates unable to read boost control register\n");
		return false;
	}
	
	numBoostStates = boostControl->getBits(0, 2, 3);
	
	free(boostControl);
	
	return numBoostStates;
}

void Interlagos::setNumBoostStates(DWORD numBoostStates)
{
	PCIRegObject *boostControl = new PCIRegObject();
	
	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("Interlagos::getNumBoostStates unable to read boost control register\n");
		return;
	}
	
	if (boostControl->getBits(0, 31, 1))
	{
		printf("Boost Lock Enabled. Cannot edit NumBoostStates\n");
		return;
	}
	
	if (boostControl->getBits(0, 7, 1))
	{
		printf("Disable boost before changing the number of boost states\n");
		return;
	}
	
	boostControl->setBits(2, 3, numBoostStates);
	
	if (!boostControl->writePCIReg())
	{
		printf("Interlagos::setNumBoostStates unable to write PCI Reg\n");
		return;
	}
	
	setBoostStates(numBoostStates);
	
	printf("Number of boosted states set to %d\n", numBoostStates);
	
	free(boostControl);
	
	return;
}

/*
 * Specifies whether CPB is enabled or disabled
 */

DWORD Interlagos::getBoost(void)
{
	PCIRegObject *boostControl = new PCIRegObject();
	DWORD boostSrc;
	
	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("Interlagos::getBoost unable to read boost control register\n");
		return -1;
	}
	
	boostSrc = boostControl->getBits(0, 0, 2);
	
	free(boostControl);
	
	if (boostSrc == 1)
		return 1;
	else if (boostSrc == 0)
		return 0;
	else
		return -1;
}

void Interlagos::setBoost(bool boost)
{	
	PCIRegObject *boostControl = new PCIRegObject();

	if (!boostControl->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x15C, getNodeMask()))
	{
		printf("Interlagos::enableBoost unable to read boost control register\n");
		free(boostControl);
		return;
	}
	
	if (boostControl->getBits(0, 31, 1))
	{
		printf("Boost Lock Enabled. Fid, Did, Vid, NodeTdp, NumBoostStates and CStateBoost limited\n");
	}
	else
	{
		printf("Boost Lock Disabled.  Unlocked processor\n");
		printf("Fid, Did, Vid, NodeTdp, NumBoostStates and CStateBoost can be edited\n");
	}

	boostControl->setBits(0, 2, boost); //Boost
	boostControl->setBits(7, 1, boost); //APM

	if (!boostControl->writePCIReg())
	{
		printf("Interlagos::enableBoost unable to write PCI Reg\n");
		free(boostControl);
		return;
	}

	if (boost)
		printf ("Boost enabled\nAPM enabled\n");
	else
		printf ("Boost disabled\nAPM disabled\n");

	free(boostControl);
}

DWORD Interlagos::getTDP(void)
{
	PCIRegObject *TDPReg = new PCIRegObject();
	PCIRegObject *TDP2Watt = new PCIRegObject();
	DWORD TDP;
	float tdpwatt;
	
	if (!TDPReg->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_LINK_CONTROL, 0x1B8, getNodeMask()))
	{
		printf("Interlagos::getTDP unable to read boost control register\n");
		return -1;
	}
	
	if (!TDP2Watt->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_5, 0xE8, getNodeMask()))
	{
		printf("Interlagos::getTDP unable to read TDP2Watt control register\n");
		return -1;
	}
	
	TDP = TDPReg->getBits(0, 0, 16);
	tdpwatt = TDP2Watt->getBits(0, 0, 10);
	tdpwatt = (tdpwatt / 1024) * TDP;
	
	printf("TDP is: %f\n",tdpwatt);
	
	setTDP(TDP);
	
	return(TDP);
}

//DRAM Timings tweaking ----------------
DWORD Interlagos::setDramTiming(DWORD device, /* 0 or 1 */
		DWORD Tcl, DWORD Trcd, DWORD Trp, DWORD Trtp, DWORD Tras, DWORD Trc,
		DWORD Twr, DWORD Trrd, DWORD Tcwl, DWORD T_mode) {

	DWORD T_mode_current;

	bool reg0, reg1, reg3, reg10;
	bool regconfhigh;

	PCIRegObject *dramConfigurationHighRegister = new PCIRegObject();
	PCIRegObject *dramTiming0 = new PCIRegObject();
	PCIRegObject *dramTiming1 = new PCIRegObject();
	PCIRegObject *dramTiming3 = new PCIRegObject();
	PCIRegObject *dramTiming10 = new PCIRegObject();

	//
	// parameter validation
	// From BKDG for Fam 15h pg 347
	//
	if (Tcl < 5 || Tcl > 0x0E)
	{
		printf("Tcl out of allowed range (5-14)\n");
		return false;
	}

	if (Trcd < 2 || Trcd > 0x13)
	{
		printf("Trcd out of allowed range (2-19)\n");
		return false;
	}

	if (Trp < 2 || Trp > 0x13)
	{
		printf("Trp out of allowed range (2-19)\n");
		return false;
	}

	if (Trtp < 4 || Trtp > 0x0A)
	{
		printf("Trtp out of allowed range (4-10)\n");
		return false;
	}
	
	if (Tras < 8 || Tras > 0x28)
	{
		printf("Tras out of allowed range (8-40)\n");
		return false;
	}
	
	if (Trrd < 1 || Trrd > 9)
	{
		printf("Trrd out of allowed range (1-9)\n");
		return false;
	}
	
	if (Trc < 0x0A || Trc > 0x38)
	{
		printf("Trrd out of allowed range (10-56)\n");
		return false;
	}

	if (T_mode < 1 || T_mode > 2)
	{
		printf("T out of allowed range (1-2)\n");
		return false;
	}
	
	if (device == 0)
	{
		regconfhigh = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask());
	}
	else
	{
		regconfhigh = dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x194, getNodeMask());

	}
	reg0 = dramTiming0->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x200, getNodeMask());
	reg1 = dramTiming1->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x204, getNodeMask());
	reg3 = dramTiming3->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x20C, getNodeMask());
	reg10 = dramTiming10->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x22C, getNodeMask());

	if (!(regconfhigh && reg0 && reg1 && reg3 && reg10))
	{
		printf("Interlagos::getDRAMTiming - unable to read PCI register\n");
		free(dramConfigurationHighRegister);
		free(dramTiming0);
		free(dramTiming1);
		free(dramTiming3);
		free(dramTiming10);
		return false;
	}
	
	if (dramConfigurationHighRegister->getBits(0, 20, 1))
	{
		T_mode_current = 2;
	}
	else
	{
		T_mode_current = 1;
	}
	
	dramTiming0->setBits(0, 5, Tcl);
	dramTiming0->setBits(8, 5, Trcd);
	dramTiming0->setBits(16, 5, Trp);
	dramTiming0->setBits(24, 6, Tras);
	
	dramTiming1->setBits(24, 4, Trtp);
	dramTiming1->setBits(0, 6, Trc);
	dramTiming1->setBits(8, 4, Trrd);
	
	dramTiming3->setBits(0, 5, Tcwl);

	dramTiming10->setBits(0, 5, Twr);

	printf ("Updating DRAM Timing0 Register... ");
	if (!dramTiming0->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");

	printf ("Updating DRAM Timing1 Register... ");
	if (!dramTiming1->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");
	
	printf ("Updating DRAM Timing3 Register... ");
	if (!dramTiming3->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");
	
	printf ("Updating DRAM Timing10 Register... ");
	if (!dramTiming10->writePCIReg())
		printf ("failed\n");
	else
		printf ("success\n");

	if (T_mode_current != T_mode)
	{
		if (T_mode == 2)
		{
			dramConfigurationHighRegister->setBits(20, 1, 1);
		}
		else
		{
			dramConfigurationHighRegister->setBits(20, 1, 0);
		}

		printf("Updating T from %uT to %uT... ", T_mode_current, T_mode);

		if (!dramConfigurationHighRegister->writePCIReg())
			printf ("failed\n");
		else
			printf ("success\n");
	}
	
	free(dramConfigurationHighRegister);
	free(dramTiming0);
	free(dramTiming1);
	free(dramTiming3);
	free(dramTiming10);

	return true;
}


//Temperature registers ------------------
DWORD Interlagos::getTctlRegister (void)
{
	PCIRegObject *pciRegObject;
	DWORD temp;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa4, getNodeMask()))
	{
		printf("Interlagos.cpp::getTctlRegister - unable to read PCI register\n");
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

DWORD Interlagos::getTctlMaxDiff()
{
	PCIRegObject *pciRegObject;
	DWORD maxDiff;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa4, getNodeMask()))
	{
		printf("Interlagos.cpp::getTctlMaxDiff unable to read PCI register\n");
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
DWORD Interlagos::getSlamTime (void)
{
	PCIRegObject *pciRegObject;
	DWORD slamTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
	{
		printf("Interlagos.cpp::getSlamTime unable to read PCI register\n");
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

void Interlagos::setSlamTime (DWORD slmTime)
{
	PCIRegObject *pciRegObject;

	if (slmTime < 0 || slmTime > 7)
	{
		printf ("Invalid Slam Time: must be between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
	{
		printf("Interlagos::setSlamTime -  unable to read PCI Register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos.cpp::setSlamTime - unable to write PCI register\n");
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
DWORD Interlagos::getStepUpRampTime (void)
{
	PCIRegObject *pciRegObject;
	DWORD vsRampTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
	{
		printf("Interlagos.cpp::getStepUpRampTime unable to read PCI Register\n");
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

	vsRampTime=pciRegObject->getBits(0, 24, 4);

	free (pciRegObject);

	return vsRampTime;
}

DWORD Interlagos::getStepDownRampTime (void)
{
	PCIRegObject *pciRegObject;
	DWORD vsRampTime;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
	{
		printf("Interlagos::getStepDownRampTime -  unable to read PCI Register\n");
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

	vsRampTime=pciRegObject->getBits(0, 20, 4);

	free (pciRegObject);

	return vsRampTime;
}

void Interlagos::setStepUpRampTime (DWORD rmpTime)
{
	PCIRegObject *pciRegObject;

		pciRegObject = new PCIRegObject();

		if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
		{
			printf("Interlagos::setStepUpRampTime unable to read PCI Register\n");
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

		if (!pciRegObject->writePCIReg())
		{
			printf ("Interlagos::setStepUpRampTime - unable to write PCI register\n");
			free (pciRegObject);
			return;
		}

		free (pciRegObject);

		return;
}

void Interlagos::setStepDownRampTime(DWORD rmpTime)
{
	PCIRegObject *pciRegObject;

	if (rmpTime < 0 || rmpTime > 0xf)
	{
		printf("Invalid Ramp Time: value must be between 0 and 15\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xd4, getNodeMask()))
	{
		printf("Interlagos::setStepDownRampTime - unable to read PCI Register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::setStepDownRampTime - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}


// AltVID - HTC Thermal features
bool Interlagos::HTCisCapable ()
{
	PCIRegObject *pciRegObject;
	DWORD isCapable;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xe8, getNodeMask()))
	{
		printf("Interlagos::HTCisCapable - unable to read PCI register\n");
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

bool Interlagos::HTCisEnabled()
{
	PCIRegObject *pciRegObject;
	DWORD isEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCisEnabled - unable to read PCI register\n");
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

bool Interlagos::HTCisActive()
{
	PCIRegObject *pciRegObject;
	DWORD isActive;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCisActive - unable to read PCI register\n");
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

bool Interlagos::HTChasBeenActive()
{
	PCIRegObject *pciRegObject;
	DWORD hasBeenActivated;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTChasBeenActive - unable to read PCI register\n");
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

DWORD Interlagos::HTCTempLimit()
{
	PCIRegObject *pciRegObject;
	DWORD tempLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCTempLimit - unable to read PCI register\n");
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

bool Interlagos::HTCSlewControl()
{
	PCIRegObject *pciRegObject;
	DWORD slewControl;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCSlewControl - unable to read PCI register\n");
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

DWORD Interlagos::HTCHystTemp()
{
	PCIRegObject *pciRegObject;
	DWORD hystTemp;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCHystTemp - unable to read PCI register\n");
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

DWORD Interlagos::HTCPStateLimit()
{
	PCIRegObject *pciRegObject;
	DWORD pStateLimit;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCPStateLimit - unable to read PCI register\n");
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

bool Interlagos::HTCLocked()
{
	PCIRegObject *pciRegObject;
	DWORD htcLocked;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCLocked - unable to read PCI register\n");
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

void Interlagos::HTCEnable()
{
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCEnable - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::HTCEnable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void Interlagos::HTCDisable()
{
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCDisable - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::HTCDisable - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void Interlagos::HTCsetTempLimit (DWORD tempLimit)
{
	PCIRegObject *pciRegObject;

	if (tempLimit < 52 || tempLimit > 115)
	{
		printf("HTCsetTempLimit: accepted range between 52 and 115\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCsetTempLimit - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::HTCsetTempLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;

}

void Interlagos::HTCsetHystLimit(DWORD hystLimit)
{
	PCIRegObject *pciRegObject;

	if (hystLimit < 0 || hystLimit > 7)
	{
		printf("HTCsetHystLimit: accepted range between 0 and 7\n");
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0x64, getNodeMask()))
	{
		printf("Interlagos::HTCsetHystLimit - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos::HTCsetHystLimit - unable to write PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

DWORD Interlagos::getAltVID()
{
	PCIRegObject *pciRegObject;
	DWORD altVid;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask()))
	{
		printf("Interlagos.cpp::getAltVID - unable to read PCI register\n");
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

void Interlagos::setAltVid(DWORD altVid)
{
	PCIRegObject *pciRegObject;

	if ((altVid < maxVID()) || (altVid > minVID()))
	{
		printf("setAltVID: VID Allowed range %d-%d\n", maxVID(), minVID());
		return;
	}

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xdc, getNodeMask()))
	{
		printf("Interlagos.cpp::setAltVID - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf("Interlagos.cpp::setAltVID - unable to write to PCI register\n");
		free(pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

// Hypertransport Link

//TODO: All hypertransport Link section must be tested and validated!!

DWORD Interlagos::getHTLinkWidth(DWORD link, DWORD Sublink, DWORD *WidthIn, DWORD *WidthOut, bool *pfCoherent, bool *pfUnganged)
{
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
	if (!linkTypeRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET, 0x98 + (0x20 * link), getNodeMask()))
	{
		printf("Interlagos::getHTLinkWidth - unable to read linkType PCI Register\n");
		free(linkTypeRegObject);
		return false;
	}

	linkControlRegObject = new PCIRegObject();
	//Link Control Register is located at 0x84 + 0x20 * link
	if (!linkControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET, 0x84 + (0x20 * link), getNodeMask()))
	{
		printf("Interlagos::getHTLinkWidth - unable to read linkControl PCI Register\n");
		free(linkTypeRegObject);
		free(linkControlRegObject);
		return false;
	}

	linkExtControlRegObject = new PCIRegObject();
	//Link Control Extended Register is located at 0x170 + 0x04 * link
	if (!linkExtControlRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET, 0x170 + (0x04 * link), getNodeMask()))
	{
		printf("Interlagos::getHTLinkWidth - unable to read linkExtendedControl PCI Register\n");
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
	if (linkTypeRegObject->getBits(0, 0, 1) == 0)
	{
		free(linkTypeRegObject);
		free(linkControlRegObject);
		free(linkExtControlRegObject);

		return 0;
	}

	//Bits 28-30 from link Control Register represent output link width
	int Out = linkControlRegObject->getBits(0, 28, 3);
	//Bits 24-26 from link Control Register represent input link width
	int In = linkControlRegObject->getBits(0, 24, 3);

	switch (Out)
	{
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

	switch (In)
	{
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

	if (Sublink == 0)
	{
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

DWORD Interlagos::getHTLinkSpeed (DWORD link, DWORD Sublink)
{
	DWORD FUNC_TARGET;

	PCIRegObject *linkRegisterRegObject = new PCIRegObject();
	DWORD linkFrequencyRegister = 0x88;

	DWORD dwReturn;

	if( Sublink == 1 )
		FUNC_TARGET=PCI_FUNC_LINK_CONTROL; //function 4
	else
		FUNC_TARGET=PCI_FUNC_HT_CONFIG; //function 0

	linkFrequencyRegister += (0x20 * link);

	if (!linkRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE,FUNC_TARGET,linkFrequencyRegister,getNodeMask()))
	{
		printf ("Interlagos::getHTLinkSpeed - unable to read linkRegister PCI Register\n");
		free (linkRegisterRegObject);
		return false;
	}

	dwReturn = linkRegisterRegObject->getBits(0,8,4); //dwReturn = (miscReg >> 8) & 0xF;

	if (getSpecModelExtended() >= 8) /* revision D or later */
	{
		DWORD linkFrequencyExtensionRegister = 0x9c;
		PCIRegObject *linkExtRegisterRegObject = new PCIRegObject();

		linkFrequencyExtensionRegister += (0x20 * link);

		if (!linkExtRegisterRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, FUNC_TARGET, linkFrequencyExtensionRegister, getNodeMask()))
		{
			printf ("Interlagos::getHTLinkSpeed - unable to read linkExtensionRegister PCI Register\n");
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

void Interlagos::printRoute(DWORD route)
{

	if (route & 0x1)
	{
		printf("this ");
	}

	if (route & 0x2)
	{
		printf("l0 s0 ");
	}

	if (route & 0x4)
	{
		printf("l1 s0 ");
	}

	if (route & 0x8)
	{
		printf("l2 s0 ");
	}

	if (route & 0x10)
	{
		printf("l3 s0 ");
	}

	if (route & 0x20)
	{
		printf("l0 s1 ");
	}

	if (route & 0x40)
	{
		printf("l1 s1 ");
	}

	if (route & 0x80)
	{
		printf("l2 s1 ");
	}

	if (route & 0x100)
	{
		printf("l3 s1 ");
	}

	printf("\n");
}


DWORD Interlagos::getHTLinkDistributionTarget(DWORD link, DWORD *DstLnk, DWORD *DstNode)
{
	//Coherent Link Traffic Distribution Register:
	PCIRegObject *cltdRegObject;

	//Routing Table register:
	PCIRegObject *routingTableRegObject;
	DWORD routingTableRegister = 0x40;

	int i;

	cltdRegObject = new PCIRegObject();

	if (!cltdRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, 0x164, getNodeMask()))
	{
		printf("Interlagos::getHTLinkDistributionTarget - unable to read Coherent Link Traffic Distribution PCI Register\n");
		free(cltdRegObject);
		return 0;
	}

	//Destination link is set in bits 16-23
	//*DstLnk = (miscReg >> 16) & 0x7F;
	*DstLnk = cltdRegObject->getBits(0, 16, 7);

	//Destination node is set in bits 8-11
	//*DstNode = (miscReg >> 8) & 0x7;
	*DstNode = cltdRegObject->getBits(0, 8, 3);

	for (i = 0; i < 8; i++)
	{
		DWORD BcRoute;
		DWORD RpRoute;
		DWORD RqRoute;

		routingTableRegObject = new PCIRegObject();
		if (!routingTableRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, routingTableRegister, getNodeMask()))
		{
			printf("Interlagos::getHTLinkDistributionTarget - unable to read Routing Table PCI Register\n");
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

void Interlagos::setHTLinkSpeed (DWORD linkRegister, DWORD reg)
{
	PCIRegObject *pciRegObject;

	if ((reg==1) || (reg==3) || (reg==15) || (reg==16) || (reg<=0) || (reg>=0x14))
	{
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

	linkRegister = 0x88 + 0x20 * linkRegister;

	pciRegObject=new PCIRegObject ();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_HT_CONFIG, linkRegister, getNodeMask()))
	{
		printf ("Interlagos.cpp::setHTLinkSpeed - unable to read PCI register\n");
		free (pciRegObject);
		return;
	}

	pciRegObject->setBits(8, 4, reg);

	if (!pciRegObject->writePCIReg())
	{
		printf ("Interlagos.cpp::setHTLinkSpeed - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free (pciRegObject);

	return;
}

// CPU Usage module
bool Interlagos::getPsiEnabled()
{
	PCIRegObject *pciRegObject;
	DWORD psiEnabled;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
	{
		printf("Interlagos.cpp::getPsiEnabled - unable to read PCI register\n");
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

DWORD Interlagos::getPsiThreshold()
{
	PCIRegObject *pciRegObject;
	DWORD psiThreshold;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
	{
		printf("Interlagos.cpp::getPsiThreshold - unable to read PCI register\n");
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

	psiThreshold = pciRegObject->getBits(0, 0, 7);

	free(pciRegObject);

	return psiThreshold;
}

void Interlagos::setPsiEnabled (bool toggle)
{
	PCIRegObject *pciRegObject;

	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
	{
		printf("Interlagos.cpp::setPsiEnabled - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf ("Interlagos.cpp::setPsiEnabled - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

void Interlagos::setPsiThreshold (DWORD threshold)
{
	PCIRegObject *pciRegObject;

	if (threshold > minVID() || threshold < maxVID())
	{
		printf ("setPsiThreshold: value must be between %d and %d\n",minVID(), maxVID());
		return;
	}


	pciRegObject = new PCIRegObject();

	if (!pciRegObject->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_MISC_CONTROL_3, 0xa0, getNodeMask()))
	{
		printf("Interlagos.cpp::setPsiThreshold - unable to read PCI register\n");
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

	if (!pciRegObject->writePCIReg())
	{
		printf ("Interlagos.cpp::setPsiThreshold - unable to write PCI register\n");
		free (pciRegObject);
		return;
	}

	free(pciRegObject);

	return;
}

// Various settings
bool Interlagos::getC1EStatus()
{
// 	MSRObject *msrObject;
// 	DWORD c1eBit;
// 
// 	msrObject = new MSRObject ();
// 
// 	if (!msrObject->readMSR(CMPHALT_REG, getMask()))
// 	{
// 		printf ("Interlagos.cpp::getC1EStatus - unable to read MSR\n");
// 		free (msrObject);
// 		return false;
// 	}
// 
// 	//Returns data for the first cpu in cpuMask (cpu 0)
// 	//C1E bit is stored in bit 28
// 	c1eBit=msrObject->getBitsLow(0, 28, 1);
// 
// 	free (msrObject);
// 
// 	return (bool) c1eBit;
// 
	// There are a number of requirements for C1E on 15h
	// These can be found on pg 85 of the BKDG for 15h
	
	printf("Not configured for 15h\n");
	
	return false;
}

void Interlagos::setC1EStatus (bool toggle)
{
// 	MSRObject *msrObject;
// 
// 	msrObject = new MSRObject ();
// 
// 	if (!msrObject->readMSR(CMPHALT_REG, getMask()))
// 	{
// 		printf ("Interlagos.cpp::setC1EStatus - unable to read MSR\n");
// 		free (msrObject);
// 		return;
// 	}
// 
// 	msrObject->setBitsLow(28, 1, toggle);
// 
// 	//C1E bit is stored in bit 28
// 	if (!msrObject->writeMSR())
// 	{
// 		printf ("Interlagos.cpp::setC1EStatus - unable to write MSR\n");
// 		free (msrObject);
// 		return;
// 	}
// 
// 	free (msrObject);
// 
// 	return;
// 
	// There are a number of requirements for C1E on 15h
	// These can be found on pg 85 of the BKDG for 15h
	
	printf("Not configured for 15h\n");
	
	return;
}

// Performance Counters

/*
 * Will show some informations about performance counter slots
 */
void Interlagos::perfCounterGetInfo()
{
	Interlagos::K10PerformanceCounters::perfCounterGetInfo(this);
}

/*
 * perfCounterGetValue will retrieve and show the performance counter value for all the selected nodes/processors
 *
 */
void Interlagos::perfCounterGetValue (unsigned int perfCounter)
{
	PerformanceCounter *performanceCounter;

	performanceCounter = new PerformanceCounter(getMask(), perfCounter, this->getMaxSlots());

	if (!performanceCounter->takeSnapshot())
	{
		printf("K10PerformanceCounters::perfCounterGetValue - unable to read performance counter");
		free (performanceCounter);
		return;
	}

	printf ("Performance counter value: (decimal)%ld (hex)%lx\n", performanceCounter->getCounter(0), performanceCounter->getCounter(0));
}

void Interlagos::perfMonitorCPUUsage()
{
	Interlagos::K10PerformanceCounters::perfMonitorCPUUsage(this);
}

void Interlagos::perfMonitorFPUUsage()
{
	Interlagos::K10PerformanceCounters::perfMonitorFPUUsage(this);
}

void Interlagos::perfMonitorDCMA()
{
	Interlagos::K10PerformanceCounters::perfMonitorDCMA(this);
}


void Interlagos::getCurrentStatus (struct procStatus *pStatus, DWORD core)
{
	DWORD eaxMsr, edxMsr;
	
	RdmsrPx (0xc0010071, &eaxMsr, &edxMsr,(DWORD_PTR) 1 << core);
	pStatus->pstate = (eaxMsr >> 16) & 0x7;
	pStatus->vid = (eaxMsr >> 9) & 0x7f;
	pStatus->fid = eaxMsr & 0x3f;
	pStatus->did = (eaxMsr >> 6) & 0x7;
	
	return;
}

void Interlagos::checkMode()
{
	DWORD a, b, c, i, j, k, pstate, vid, fid, did;
	DWORD eaxMsr, edxMsr;
	DWORD timestamp;
	DWORD states[processorNodes][processorCores][getPowerStates()];
	DWORD savedstates[processorNodes][processorCores][getPowerStates()];
	DWORD minTemp, maxTemp, temp, savedMinTemp, savedMaxTemp;
	DWORD oTimeStamp, iTimeStamp;
	float curVcore;
	DWORD maxPState;

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

	while(1)
	{
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

		if ((timestamp-oTimeStamp)>30000)
		{
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


bool Interlagos::setDramController(DWORD device)
{
	PCIRegObject *dctConfigurationSelect;
	
	dctConfigurationSelect = new PCIRegObject();

	if (!dctConfigurationSelect->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_ADDRESS_MAP, 0x10C, getNodeMask())) {
		fprintf(stderr, "Interlagos::setDramController -- readPCIReg failed\n");
		delete dctConfigurationSelect;
		return false;
	}
	dctConfigurationSelect->setBits(0, 1, device);
	dctConfigurationSelect->setBits(4, 2, 0); /* NB P-state 0 */
	if (!dctConfigurationSelect->writePCIReg()) {
		fprintf(stderr, "Interlagos::setDramController -- writePCIReg failed\n");
		delete dctConfigurationSelect;
		return false;
	}
	delete dctConfigurationSelect;
	return true;
}

bool Interlagos::getDramValid (DWORD device)
{
	PCIRegObject *dramConfigurationHighRegister;
	DWORD ret;

	if (!setDramController(device)) {
		return false;
	}

	dramConfigurationHighRegister = new PCIRegObject();

	if (!dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask()))
	{
		printf("Interlagos::getDramValid - unable to read PCI registers\n");
		delete dramConfigurationHighRegister;
		return false;
	}

	ret = dramConfigurationHighRegister->getBits(0, 7, 1);
	delete dramConfigurationHighRegister;
	return ret;
}

/*
 * 04h = 667
 * 06h = 800
 * 0Ah = 1066
 * 0Eh = 1333
 * 12h = 1600
 * 16h = 1866
 * 1Ah = 2133
 * 1Eh = 2400
 */
int Interlagos::getDramFrequency (DWORD device, DWORD *T_mode)
{
	PCIRegObject *dramConfigurationHighRegister;
	DWORD regValue;

	if (!setDramController(device)) {
		return 0;
	}

	dramConfigurationHighRegister = new PCIRegObject ();

	if (!dramConfigurationHighRegister->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x94, getNodeMask()))
	{
		printf("Interlagos::getDRAMFrequency - unable to read PCI registers\n");
		delete dramConfigurationHighRegister;
		return 0;
	}
	
	if (dramConfigurationHighRegister->getBits(0, 20, 1))
	{
		*T_mode = 2;
	}
	else
	{
		*T_mode = 1;
	}
	
	regValue = dramConfigurationHighRegister->getBits(0, 0, 5);
	delete dramConfigurationHighRegister;
	
	switch (regValue)
	{
		case 0x04:
			return 667;
		case 0x06:
			return 800;
		case 0x0A:
			return 1066;
		case 0x0E:
			return 1333;
		case 0x12:
			return 1600;
		case 0x16:
			return 1866;
		case 0x1A:
			return 2133;
		case 0x1E:
			return 2400;
		default:
			return 0;
	}
}

void Interlagos::getDramTiming(DWORD device, /* 0 or 1   DCT0 or DCT1 */
		DWORD *Tcl, DWORD *Trcd, DWORD *Trp, DWORD *Trtp, DWORD *Tras,
		DWORD *Trc, DWORD *Twr, DWORD *Trrd, DWORD *Tcwl, DWORD *Tfaw,
		DWORD *TrwtWB, DWORD *TrwtTO, DWORD *Twtr, DWORD *Twrrd, DWORD *Twrwrsdsc,
		DWORD *Trdrdsdsc, DWORD *Tref, DWORD *Trfc0, DWORD *Trfc1, DWORD *Trfc2,
		DWORD *Trfc3, DWORD *MaxRdLatency)
{
	bool reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg10;
	bool dramnbpstate, reghigh;

	if (!setDramController(device)) {
		return;
	}

	PCIRegObject *dramTimingHigh = new PCIRegObject();
	PCIRegObject *dramTiming0 = new PCIRegObject();
	PCIRegObject *dramTiming1 = new PCIRegObject();
	PCIRegObject *dramTiming2 = new PCIRegObject();
	PCIRegObject *dramTiming3 = new PCIRegObject();
	PCIRegObject *dramNBPState = new PCIRegObject();
	PCIRegObject *dramTiming4 = new PCIRegObject();
	PCIRegObject *dramTiming5 = new PCIRegObject();
	PCIRegObject *dramTiming6 = new PCIRegObject();
	PCIRegObject *dramTiming10 = new PCIRegObject();

	//Single configuration register for DRAM, unlike K10
	//
	reghigh = dramTimingHigh->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x8C, getNodeMask());

	reg0 = dramTiming0->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x200, getNodeMask());
	reg1 = dramTiming1->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x204, getNodeMask());
	reg2 = dramTiming2->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x208, getNodeMask());
	reg3 = dramTiming3->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x20C, getNodeMask());
	dramnbpstate = dramNBPState->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x210, getNodeMask());
	reg4 = dramTiming4->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x214, getNodeMask());
	reg5 = dramTiming5->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x218, getNodeMask());
	reg6 = dramTiming6->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x21C, getNodeMask());
	reg10 = dramTiming10->readPCIReg(PCI_DEV_NORTHBRIDGE, PCI_FUNC_DRAM_CONTROLLER, 0x22C, getNodeMask());

	if (!(reghigh && reg0 && reg1 && reg2 && reg3 && reg4 && reg5 && reg6 && reg10 && dramnbpstate))
	{
		printf("Interlagos::getDRAMTiming - unable to read PCI register\n");
		free(dramTimingHigh);
		free(dramTiming0);
		free(dramTiming1);
		free(dramTiming2);
		free(dramTiming3);
		free(dramNBPState);
		free(dramTiming4);
		free(dramTiming5);
		free(dramTiming6);
		free(dramTiming10);
		return;
	}
	
	*Tref = dramTimingHigh->getBits(0, 16, 2);
	
	*Tras = dramTiming0->getBits(0, 24, 6);
	*Trp = dramTiming0->getBits(0, 16, 5);
	*Trcd = dramTiming0->getBits(0, 8, 5);
	*Tcl = dramTiming0->getBits(0, 0, 5);
	
	*Trtp = dramTiming1->getBits(0, 24, 4);
	*Trrd = dramTiming1->getBits(0, 8, 4);
	*Trc = dramTiming1->getBits(0, 0, 6);
	*Tfaw = dramTiming1->getBits(0, 16, 6);
	
	*Trfc0 = dramTiming2->getBits(0, 0, 3);
	*Trfc1 = dramTiming2->getBits(0, 8, 3);
	*Trfc2 = dramTiming2->getBits(0, 16, 3);
	*Trfc3 = dramTiming2->getBits(0, 24, 3);
	
	*Twtr = dramTiming3->getBits(0, 8, 4);
	*Tcwl = dramTiming3->getBits(0, 0, 5);
	
	*MaxRdLatency = dramNBPState->getBits(0, 22, 10);
	
	*Twrwrsdsc = dramTiming4->getBits(0, 16, 4);
	
	*Twrrd = dramTiming5->getBits(0, 8, 4);
	*Trdrdsdsc = dramTiming5->getBits(0, 24, 4);
	
	*TrwtTO = dramTiming6->getBits(0, 8, 5);
	*TrwtWB = dramTiming6->getBits(0, 16, 5);
	
	*Twr = dramTiming10->getBits(0, 0, 5);
	
	free(dramTimingHigh);
	free(dramTiming0);
	free(dramTiming1);
	free(dramTiming2);
	free(dramTiming3);
	free(dramNBPState);
	free(dramTiming4);
	free(dramTiming5);
	free(dramTiming6);
	free(dramTiming10);
	
	return;
}


/************** PUBLIC SHOW METHODS ******************/


void Interlagos::showHTLink()
{
	int nodes = getProcessorNodes();
	int i;

	printf("\nHypertransport Status:\n");

	for (i = 0; i < nodes; i++)
	{
		setNode(i);
		//DWORD DstLnk, DstNode;
		int linknumber;

		for (linknumber = 0; linknumber < 4; linknumber++)
		{
			int HTLinkSpeed;
			DWORD WidthIn;
			DWORD WidthOut;
			bool fCoherent;
			bool fUnganged;
			DWORD Sublink = 0;

			getHTLinkWidth(linknumber, Sublink, &WidthIn, &WidthOut, &fCoherent, &fUnganged);

			if (WidthIn == 0 || WidthOut == 0)
			{
				printf("Node %u Link %u Sublink %u not connected\n", i, linknumber, Sublink);
				continue;
			}

			HTLinkSpeed = getHTLinkSpeed(linknumber, Sublink);

			printf("Node %u Link %u Sublink %u Bits=%u Coh=%u SpeedReg=%d (%dMHz)\n", 
					i, linknumber, Sublink, WidthIn, fCoherent,
					//DstLnk,
					//DstNode,
					HTLinkSpeed, HTLinkToFreq(HTLinkSpeed));

			//
			// no sublinks.
			//

			if (!fUnganged)
			{
				continue;
			}

			Sublink = 1;

			getHTLinkWidth(linknumber, Sublink, &WidthIn, &WidthOut, &fCoherent, &fUnganged);

			if (WidthIn == 0 || WidthOut == 0)
			{
				printf("Node %u Link %u Sublink %u not connected\n", i, linknumber, Sublink);
				continue;
			}

			HTLinkSpeed = getHTLinkSpeed(linknumber, Sublink);
			printf("Node %u Link %u Sublink %u Bits=%u Coh=%u SpeedReg=%d (%dMHz)\n",
					i, linknumber, Sublink, WidthIn, fCoherent,
					//DstLnk,
					//DstNode,
					HTLinkSpeed, HTLinkToFreq(HTLinkSpeed));

		}

		// p->getHTLinkDistributionTargetByNode(i, 0, &DstLnk, &DstNode);

		printf("\n");
	}
}

void Interlagos::showHTC()
{
	int i;
	int nodes = getProcessorNodes();

	printf("\nHardware Thermal Control Status:\n\n");

	if (HTCisCapable() != true)
	{
		printf("Processor is not HTC Capable\n");
		return;
	}

	for (i = 0; i < nodes; i++)
	{
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

void Interlagos::showDramTimings()
{
	int nodes = getProcessorNodes();
	int node_index;
	int dct_index;
	DWORD Tcl, Trcd, Trp, Trtp, Tras, Trc, Twr, Trrd, Tcwl, T_mode;
	DWORD Tfaw, TrwtWB, TrwtTO, Twtr, Twrrd, Twrwrsdsc, Trdrdsdsc, Tref, Trfc0;
	DWORD Trfc1, Trfc2, Trfc3, MaxRdLatency;
	DWORD ddrFrequency;

	printf ("\nDRAM Configuration Status\n\n");

	for (node_index = 0; node_index < nodes; node_index++)
	{
		setNode (node_index);
		printf ("Node %u ---\n", node_index);

		for (dct_index = 0; dct_index < 2; dct_index++)
		{
			if (getDramValid(dct_index))
			{
				ddrFrequency = getDramFrequency(dct_index, &T_mode);

				getDramTiming(dct_index, &Tcl, &Trcd, &Trp, &Trtp, &Tras, &Trc, &Twr, &Trrd, &Tcwl, &Tfaw, 
						 &TrwtWB, &TrwtTO, &Twtr, &Twrrd, &Twrwrsdsc, &Trdrdsdsc, &Tref, &Trfc0, &Trfc1, &Trfc2, &Trfc3, &MaxRdLatency);
				
				printf ("DCT%d: DDR3 frequency: %d MHz\n",dct_index, ddrFrequency);

				//DRAM Registers
				printf("Tcl=%u Trcd=%u Trp=%u Tras=%u Access Mode:%uT Trtp=%u Trc=%u Twr=%u Trrd=%u Tcwl=%u Tfaw=%u\n",
						Tcl, Trcd, Trp, Tras, T_mode, Trtp, Trc, Twr, Trrd, Tcwl,
						Tfaw);

				printf("TrwtWB=%u TrwtTO=%u Twtr=%u Twrrd=%u Twrwrsdsc=%u Trdrdsdsc=%u Tref=%u Trfc0=%u Trfc1=%u Trfc2=%u Trfc3=%u MaxRdLatency=%u\n",
						TrwtWB, TrwtTO, Twtr, Twrrd, Twrwrsdsc, Trdrdsdsc, Tref, Trfc0,
						Trfc1, Trfc2, Trfc3, MaxRdLatency);

				for (int i = 0; i < 8; i++) {
					unsigned int val;
					PCIRegObject *csbaseaddr = new PCIRegObject();
					setDramController(dct_index);
					csbaseaddr->readPCIReg(PCI_DEV_NORTHBRIDGE,
                                                PCI_FUNC_DRAM_CONTROLLER, 0x40 + 4 * i, 1 << node_index);
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
			}
			else
			{
				printf ("DCT%d: - controller inactive -\n", dct_index);
			}
			printf("\n");
		}

		printf("\n");
	}
	return;
}
