#include <stdio.h>
#include <stdlib.h>
#include "TurionPowerControl.h"

//This program is supposed to compile with GCC under Linux x86/x86-64 platforms
//and with Microsoft Visual C++ 2005/2008 under Windows x86/x86-64 platforms

#ifdef _WIN32
	#include <windows.h>
	#include "OlsApi.h"
	#include "OlsDef.h"
	#include <tchar.h>
#endif

#ifdef __linux
	#include "cpuPrimitives.h"
#endif

#include <string.h>

//Main include for processor definitions:
#include "Processor.h"

//Include for processor families:
#include "Griffin.h"
#include "K10Processor.h"
#include "Brazos.h"
#include "Llano.h"
#include "Interlagos.h"

#include "config.h"
#include "scaler.h"

#include "source_version.h"
#include "version.h"

#ifdef _WIN32
int initWinRing0 () {

	int dllStatus;
	BYTE verMajor,verMinor,verRevision,verRelease;

	InitializeOls ();

	dllStatus=GetDllStatus ();
	
	if (dllStatus!=0) {
		printf ("Unable to initialize WinRing0 library\n");

		switch (dllStatus) {
			case OLS_DLL_UNSUPPORTED_PLATFORM:
				printf ("Error: unsupported platform\n");
				break;
			case OLS_DLL_DRIVER_NOT_LOADED:
				printf ("Error: driver not loaded\n");
				break;
			case OLS_DLL_DRIVER_NOT_FOUND:
				printf ("Error: driver not found\n");
				break;
			case OLS_DLL_DRIVER_UNLOADED:
				printf ("Error: driver unloaded by other process\n");
				break;
			case OLS_DLL_DRIVER_NOT_LOADED_ON_NETWORK:
				printf ("Error: driver not loaded from network\n");
				break;
			case OLS_DLL_UNKNOWN_ERROR:
				printf ("Error: unknown error\n");
				break;
			default:
				printf ("Error: unknown error\n");
		}

		return false;
	}

	GetDriverVersion (&verMajor,&verMinor,&verRevision,&verRelease);

	if ((verMajor>=1) && (verMinor>=2)) return true;

	return false;

}

int closeWinRing0 () {

	DeinitializeOls ();	

	return true;

}
#endif

#ifdef __linux
int initWinRing0 () {
	return true;
}

int closeWinRing0 () {
	return true;
}
#endif


//Converts an argument to integer
int arg2i (const char **argV,int arg) {
	return atoi(argV[arg]);
}

//Checks for all modules available and returns the right Processor object
//for current system. If there isn't a valid module, returns null
Processor *getSupportedProcessor () {

	if (K10Processor::isProcessorSupported()) {
		return (class Processor *)new K10Processor ();
	}

	if (Griffin::isProcessorSupported()) {
		return (class Processor *)new Griffin();
	}

	if (Brazos::isProcessorSupported()) {
		return (class Processor *)new Brazos();
	}

	if (Llano::isProcessorSupported()) {
				return (class Processor *)new Llano();
	}
	if (Interlagos::isProcessorSupported()) {
				return (class Processor *)new Interlagos();
	}

	/*TODO: This code should be moved somewhere else than here:
	 *

	#ifdef _WIN32
		printf ("-> Check that you are running the program with administrative privileges.\n");
	#endif
	#ifdef __GNUC__	
		printf ("-> Check that you are running the program as root\n");	
		printf ("-> Also check that cpuid and msr modules are properly loaded\n");
		printf ("-> and and functioning (see the included manual, Chapter 5)\n");
	#endif

	*/

	return NULL;

}

void processorStatus (Processor *p) {

	PState ps(0);
	int i,k,j;

	int cores=p->getProcessorCores();
	int pstates=p->getPowerStates();
	int nodes=p->getProcessorNodes();
	int boost = p->getBoostStates();

	printf ("Main processor is %s\n",p->getProcessorStrId());

	printf ("\tFamily: 0x%x\t\tModel: 0x%x\t\tStepping: 0x%x\n",p->getSpecFamilyBase(),p->getSpecModel(),p->getSpecStepping());
	printf ("\tExtended Family: 0x%x\tExtended Model: 0x%x\n",p->getSpecFamilyExtended(),p->getSpecModelExtended());
	printf ("\tPackage Type: 0x%x\tBrandId: 0x%x\t\n",p->getSpecPkgType(),p->getSpecBrandId());
	
	printf ("Machine has %d nodes\n",p->getProcessorNodes());
	printf ("Processor has %d cores\n",cores);
	printf ("Processor has %d p-states\n",pstates);
	printf ("Processor has %d boost states\n",boost);
	printf ("\nPower States table:\n");

	for (j = 0; j < nodes; j++)
	{
		for (i = 0; i < cores; i++)
		{
			printf("-- Node: %d Core %d\n", j, i);
			for (k = 0; k < pstates; k++)
			{
				ps.setPState(k);
				p->setCore(i);
				p->setNode(j);

				printf("core %d ", i);
				printf("pstate %d ", k);
				if (boost > 0 && k < boost)
					printf("(pb%d) - ",k);
				else if (boost > 0 && k >= boost)
					printf("(p%d) - ",(k - boost));
				else
					printf("(p%d) - ",k);
				
				if (!(p->getBoost()) && k < boost)
				{
					printf ("Boost PState Disabled");
				}
				else
				{
					printf("En:%d ", p->pStateEnabled(ps.getPState()));
					printf("VID:%d ", p->getVID(ps.getPState()));
					if (p->getFID(ps.getPState())!=-1) printf("FID:%0.0f ", p->getFID(ps.getPState()));
					if (p->getDID(ps.getPState())!=-1) printf("DID:%0.2f ", p->getDID(ps.getPState()));
					printf("Freq:%d ", p->getFrequency(ps.getPState()));
					printf("VCore:%0.4f", p->getVCore(ps.getPState()));
				}
				printf("\n");
			}
		}
	}

	for (j=0;j<p->getProcessorNodes();j++)
	{
		p->setNode (j);
		p->setCore (0);
		printf ("\n --- Node %u:\n", p->getNode());
		printf ("Processor Maximum PState: ");
		if (p->getMaximumPState().getPState() == 0 )
			printf ("unsupported feature\n");
		else
			printf ("%d\n",p->getMaximumPState().getPState());
		printf ("Processor Startup PState: %d\n", p->startupPState());
		printf ("Processor Maximum Operating Frequency: ");
		
		if (p->maxCPUFrequency()==0)
			printf ("No maximum defined. Unlocked multiplier.\n");
		else
			printf ("%d MHz\n", p->maxCPUFrequency());

		printf ("\nMinimum allowed VID: %d (%0.4fV) - Maximum allowed VID %d (%0.4fV)\n", p->minVID(),
			p->convertVIDtoVcore(p->minVID()),p->maxVID(),p->convertVIDtoVcore(p->maxVID()));
		printf ("Processor AltVID: %d (%0.4fV)\n",p->getAltVID(),p->convertVIDtoVcore(p->getAltVID()));
	}

}

void processorTempMonitoring (Processor *p) {

	unsigned int node, core;

	printf("Detected processor: %s\n", p->getProcessorStrId());

	printf("Machine has %d nodes\n", p->getProcessorNodes());
	printf("Processor has %d cores\n", p->getProcessorCores());
	printf("Processor has %d p-states\n", p->getPowerStates());
	printf("Processor has %d boost states\n", p->getBoostStates());

	printf("Processor temperature slew rate:");
	switch (p->getTctlMaxDiff()) {

	case 0:
		printf("slewing disabled\n");
		break;

	case 1:
		printf("1.0°C\n");
		break;

	case 2:
		printf("3.0°C\n");
		break;

	case 3:
		printf("9.0°C\n");
		break;

	default:
		printf("unknown\n");
		break;

	}

	printf ("\nTemperature table (monitoring):\n");

	while (1)
	{
		for (node = 0; node < p->getProcessorNodes(); node++)
		{
			printf("\nNode %d\t", node);
			for (core = 0; core < p->getProcessorCores(); core++)
			{
				p->setNode(node);
				p->setCore(core);
				printf("C%d:%d\t", core, p->getTctlRegister());
			}
		}
		printf("\n");
		Sleep(100);
		Sleep(900);
	};

	return;
}

void processorTempStatus(Processor *p) {

	unsigned int node, core;

	printf("Detected processor: %s\n", p->getProcessorStrId());

	printf("Machine has %d nodes\n", p->getProcessorNodes());
	printf("Processor has %d cores\n", p->getProcessorCores());
	printf("Processor has %d p-states\n", p->getPowerStates());
	printf("Processor has %d boost states\n", p->getBoostStates());

	printf("Processor temperature slew rate:");
	switch (p->getTctlMaxDiff()) {

	case 0:
		printf("slewing disabled\n");
		break;

	case 1:
		printf("1.0°C\n");
		break;

	case 2:
		printf("3.0°C\n");
		break;

	case 3:
		printf("9.0°C\n");
		break;

	default:
		printf("unknown\n");
		break;

	}

	printf ("\nTemperature table:\n");

	for (node = 0; node < p->getProcessorNodes(); node++) {
		printf("Node %d\t", node);
		for (core = 0; core < p->getProcessorCores(); core++) {
			p->setNode(node);
			p->setCore(core);
			printf("C%d:%d\t", core, p->getTctlRegister());
		}
		printf("\n");
	}

	return;

}

void printUsage (const char *name) {
	printf ("\nUsage: %s [options]\n", name);
	printf ("Options:\n\n");
	printf ("\t ----- Various information about processor states\n\n");
	printf (" -l\n\tLists power states\n\n");
	printf (" -spec\n\tLists detailed processor family specifications\n\n");
	printf (" -dram\n\tLists detailed DRAM timings\n\n");
	printf (" -htc\n\tShows Hardware Thermal Control status\n\n");
	printf (" -htstatus\n\tShows Hypertransport status\n\n");
	printf ("\t ----- PState VID, FID, DID manipulation -----\n\n");
	printf (" -node <nodeId>\n\tSet the active operating node. Use \"all\" to affect all nodes\n\tin the system. "
			"By default all nodes in the system are selected.\n\tIf your system has a single processor you can safely ignore\n\t"
			"this switch\n\n");
	printf (" -core <coreId>\n\tSet the active operating core. Use \"all\" to affect all cores\n\tin the current node.\n\t"
			"By default all cores in the system are selected.\n\n");
	printf (" -en <pstateId>\n\tEnables a specified (pStateId) for active cores and active nodes\n\n");
	printf (" -di <pstateId>\n\tDisables a specified (pStateId) for active cores and active nodes\n\n");
	printf (" -fo <pstateId>\n\tForce a pstate transition to specified (pStateId) for active cores\n\tand active nodes\n\n");
	printf (" -psmax <pstateMaxId>\n\tSet maximum Power State for active nodes\n\n");
	printf (" -bst <numBoostStates>\n\tSet the number of boosted states in an unlocked processor\n\n");

	printf (" -set <commands>\n\tUseful switch to set frequency and voltage without manual\n");
	printf ("\tmanipulation of FID, DID and VID values. Check the documentation\n");
	printf ("\tfor some easy example usages\n\n");

	printf ("\t ----- Voltage control -----\n\n");	
	printf (" -slamtime <value>\n -altvidslamtime <value>\n\tSet, on active nodes, vsSlamTime or vsAltVIDSlamTime for transitions \n");
	printf ("\tto AltVID state. It is the the stabilization time between\n");
	printf ("\ta SVI Voltage command and a FID change. Values range from 0 to 7 \n");
	printf ("\tand corresponds to:\n\n");
	printf ("\t\t0=10us\n\t\t1=20us\n\t\t2=30us\n\t\t3=40us\n\t\t4=60us\n\t\t5=100us\n\t\t6=200us\n\t\t7=500us\n\n");
	printf (" -gettdp\n\tReturns the current TDP for the processor\n\n");

	printf (" -rampuptime <value>\n -rampdowntime <value>\n\tSet, on active nodes, the StepUpTime or StepDownTime. \n");
	printf ("\tIt is not documented in Turion (Family 11h) datasheet, but only in\n");
	printf ("\tPhenom datasheet (Family 10h) with referring to desktop and mobile\n");
	printf ("\tprocessors. Values range from 0 to 15 and corresponds to:\n\n");
	printf ("\t\t0=400ns\t1=300ns\t2=200ns\t3=100ns\n\t\t4=90ns\t5=80ns\t6=70ns\t7=60ns\n\t\t8=50ns\t9=45ns\t10=40ns\t11=35ns\n\t\t");
	printf ("12=30ns\t13=25ns\t14=20ns\t15=15ns\n\n");

	printf ("\t ----- Northbridge (IMC) features -----\n\n");
	printf ("\t For family 11h processors:\n");
	printf (" -nbvid <vidId>\n\tSet Northbridge VID for active nodes\n\n");
	printf ("\t For family 10h processors:\n");
	printf (" -nbvid <pstateId> <vidId>\n\tSet Northbridge VID to pstateId for all cores on active nodes\n\n");
	printf (" -nbdid <pstateId> <didId>\n\tSet divisor didID to northbridge for all cores on active nodes.\n\t");
	printf ("Usually causes the processor to lock to slowest pstate. Accepted\n\tvalues are 0 and 1)\n\n");
	printf (" -nbfid <fidId>\n\tSet northbridge frequency ID to fidId\n\n");

	printf ("\t ----- Processor Temperature monitoring -----\n\n");
	printf (" -temp \n\tShow temperature registers data\n\n");
	printf (" -mtemp \n\tCostantly monitors processor temperatures\n\n");

	printf ("\t ----- Hardware Thermal Control -----\n\n");
	printf (" -htc\n\tShow information about Hardware Thermal Control status for\n\tactive nodes\n\n");
	printf (" -htcenable\n\tEnables HTC features for active nodes\n\n");
	printf (" -htcdisable\n\tDisables HTC features for active nodes\n\n");
	printf (" -htctemplimit <degrees>\n\tSet HTC high temperature limit for active nodes\n\n");
	printf (" -htchystlimit <degrees>\n\tSet HTC hysteresis (exits from HTC state) temperature\n\tlimit for active nodes\n\n");
	printf (" -altvid <vid>\n\tSet voltage identifier for AltVID status for active nodes, invoked in \n\tlow-consumption mode or during HTC active status\n\n");
	
	printf ("\t ----- PSI_L bit -----\n\n");
	printf (" -psienable\n\tEnables PSI_L bit for active nodes for improved Power Regulation\n\twith low loads\n\n");
	printf (" -psidisable\n\tDisables PSI_L bit for active nodes\n\n");
	printf (" -psithreshold <vid>\n\tSets a specified VID as a threshold for active nodes.\n\tWhen processor goes ");
	printf ("in a pstate with higher or equal vid, \n\tVRM is instructed");
	printf (" to go in power management mode\n\n");

	printf ("\t ----- Hypertransport Link -----\n\n");
	printf (" -htstatus\n\tShows Hypertransport status\n\n");
	printf (" -htset <link> <speedReg>\n\tSet the hypertransport link frequency register\n\n");
	
	printf ("\t ----- Various and others -----\n\n");
	printf (" -c1eenable\n\tSets C1E on Cmp Halt bit enabled for active nodes and active cores\n\n");
	printf (" -c1edisable\n\tSets C1E on Cmp Halt bit disabled for active nodes and active cores\n\n");
	printf (" -boostenable\n\tEnable Boost for supported processors\n\n");
	printf (" -boostdisable\n\tDisable Boost for supported processors\n\n");


	printf ("\t ----- Performance Counters -----\n\n");
	printf (" -pcgetinfo\n\tShows various informations about Performance Counters\n\n");
	printf (" -pcgetvalue <counter>\n\tShows the raw value of a specific performance counter\n\tslot of a specific core\n\n");
	printf (" -perf-cpuusage\n\tCostantly monitors CPU Usage using performance counters\n\n");
	printf (" -perf-fpuusage\n\tCostantly monitors FPU Usage using performance counters\n\n");
	printf (" -perf-dcma\n\tCostantly monitors Data Cache Misaligned Accesses\n\n");

	printf ("\t ----- Daemon Mode -----\n\n");
	printf (" -autorecall\n\tSet up daemon mode, autorecalling command line parameters\n\tevery 60 seconds\n\n");
	printf (" -scaler\n\tSet up CPU Scaler mode. In this mode TurionPowerControl takes\n\t");
	printf ("care of CPU power management and power state transitions.\n\t");
	printf ("OS Scaler must be disable for reliable operation\n\n");
	printf (" -CM\n\tEnabled Costant Monitor of frequency, voltage and pstate. Also will\n\t");
	printf ("show every anomalous transition over pstate maximum register (useful to\n\t");
	printf ("report pstate 6/7 anomalous transitions)\n\n");
	
	printf ("\t ----- Configuration File -----\n\n");
	printf (" -cfgfile <file.cfg>\n\tImports configuration from a text based configuration file\n\t");
	printf ("(see the attached example configuration file for details)\n\n");

}

//Function used by parseSetCommand to obtain a valid integer.
//The integer is put in output pointer if the function returns false (false
//means no error)
//Instead, if there is an error (no valid integer, a string where there 
//should be a value, ...) , the function returns true.
bool requireInteger (int argc, const char **argv, int offset, int *output) {

	const char *argument;
	int value;

	if (offset>=argc) return true;

	argument=argv[offset];

	value=atoi (argument);

	if (value==0)
		if (strcmp(argument,"0")!=0) return true;

	*output=value;

	return false;

}

//Equal as above, but with unsigned integer values
bool requireUnsignedInteger (int argc, const char **argv, int offset, unsigned int *output) {

	const char *argument;
	unsigned int value;

	if (offset>=argc) return true;

	argument=argv[offset];

	value=(unsigned int) atoi (argument);

	if (value==0)
		if (strcmp(argument,"0")!=0) return true;

	*output=value;

	return false;

}


//Function used by parseSetCommand to obtain a valid float.
//The float is put in output pointer if the function returns false (false
//means no error)
//Instead, if there is an error (no valid float, a string where there 
//should be a value, ...) , the function returns true.
bool requireFloat (int argc, const char **argv, int offset, float *output) {

	const char *argument;
	float value;

	if (offset>=argc) return true;

	argument=argv[offset];

	value=atof (argument);

	if (value==0)
		if (strcmp(argument,"0")!=0) return true;

	*output=value;

	return false;

}

//Simple method used by parseSetCommand to show some useful and tidy informations.
void print_stat (Processor *p, PState ps, const char *what, float value) {
		if (p->getNode()==p->ALL_NODES) printf ("All nodes "); else printf ("Node: %d ",p->getNode());
		if (p->getCore()==p->ALL_CORES) printf ("all cores "); else printf ("core: %d ",p->getCore());
		printf ("pstate %d - ", ps.getPState());
		printf ("set %s to %0.4f", what, value);
		return;
}

//This procedure parse the -set switch
int parseSetCommand (Processor *p, int argc, const char **argv, int argcOffset) {

	PState ps(0);
	unsigned int core=-1; //if core is set to -1, we will assume that the command is for all
		//the cores.
	unsigned int node=-1; //if node is set to -1, we will assume that the command is for all
		//the nodes in the system
	const char *currentCommand;

	unsigned int frequency;
	float fid, did;
	unsigned int vid;
	float voltage;
	float nbvoltage;
	unsigned int tempInt;

	p->setCore(p->ALL_CORES);
	p->setNode(p->ALL_NODES);

	do {

		//If we exceed the commands, get out of the loop
		if (argcOffset>=argc) break;

		currentCommand=argv[argcOffset];

		//If we read a command that starts with -, it means
		//that we read a new switch, so we get out of the loop
		if (currentCommand[0]=='-') break;

		/*
		 * Following section will alter operating pstate. By default, pstate 0 is set
		 */
		if ((strcmp (currentCommand,"pstate")==0) ||
			strcmp (currentCommand,"ps")==0) {

			argcOffset++;

			if (!requireUnsignedInteger(argc,argv,argcOffset,&tempInt)) {
				ps.setPState (atoi(argv[argcOffset]));
				argcOffset++;
			} else {
				printf (" pstate: expecting a valid integer\n");
			}

		/*
		 * Following section will alter operating core. Default setting is core=all
		 */
		} else if (strcmp(currentCommand,"core")==0) {

			argcOffset++;

			if (strcmp ((const char*)argv[argcOffset],"all")==0) {
				p->setCore(p->ALL_CORES);
				argcOffset++;
			}
			else
			{
				if (!requireUnsignedInteger(argc,argv,argcOffset,&core)) {
					p->setCore (core);
					argcOffset++;
				}
				else
					printf ("core: expecting a valid integer or value 'all'\n");

			}

		/*
		 * Following section will alter operating node. Default setting is node=all
		 */
		} else if (strcmp(currentCommand, "node") == 0) {

			argcOffset++;

			if (strcmp((const char*) argv[argcOffset], "all") == 0) {
				p->setNode(p->ALL_CORES);
				argcOffset++;
			} else {
				if (!requireUnsignedInteger(argc, argv, argcOffset, &node)) {
					p->setNode(node);
					argcOffset++;
				} else
					printf("node: expecting a valid integer or value 'all'\n");

			}

		/*
		 * Following section will set a new frequency for selected pstate/core/node
		 */
		} else if ((strcmp(currentCommand,"freq")==0) || 
			(strcmp(currentCommand,"f")==0) || 
			(strcmp(currentCommand,"frequency")==0)) {

			argcOffset++;

			if (!requireUnsignedInteger(argc,argv,argcOffset, &frequency)) {

				argcOffset++;
				print_stat (p,ps,"frequency",frequency);
				p->setFrequency(ps, frequency);
				if (p->getFrequency(ps)!=frequency) printf (" (rounded to %d)",p->getFrequency(ps));

			} else {
				
				printf ("frequency: expecting a valid integer\n");

			}
			
		/*
		 * Following section will set a new core voltage for operating pstate/core/node
		 */
		} else if ((strcmp(currentCommand,"vcore")==0) || 
			(strcmp(currentCommand,"vc")==0) ||
			(strcmp(currentCommand,"voltage")==0)) {
			
			argcOffset++;

			if (!requireFloat(argc,argv,argcOffset,&voltage)) {

				argcOffset++;
				print_stat (p,ps,"core voltage",voltage);
				p->setVCore (ps, voltage);
				if (p->getVCore(ps)!=voltage) printf (" (rounded to %0.4fV)",p->getVCore(ps));

			} else {
	
				printf ("vcore: expecting a valid float\n");

			}

		/*
		 * Following section will set a new northbirdge voltage for operating
		 * core on operating node
		 */
		} else if ((strcmp(currentCommand,"nbvoltage")==0) || 
			(strcmp(currentCommand,"nbv")==0) ||
			(strcmp(currentCommand,"nbvolt")==0)) {
			
			argcOffset++;

			if (!requireFloat(argc,argv,argcOffset,&nbvoltage)) {

				argcOffset++;

				//Since family 10h and family 11h differs on northbridge voltage handling, we have to make a difference
				//here. On family 10h changing northbridge voltage changes just to the pstate the user is manipulating
				//instead on family 11h it changes the northbridge voltage independently of the pstate the user is
				//manipulating
				if (p->getProcessorIdentifier()==PROCESSOR_10H_FAMILY) {

					print_stat (p,ps,"nbvoltage",nbvoltage);
					p->setNBVid (ps, p->convertVcoretoVID(nbvoltage));
					if (p->convertVIDtoVcore(p->getNBVid(ps,0))!=nbvoltage) printf (" (rounded to %0.4fV)",p->convertVIDtoVcore(p->getNBVid(ps,0)));

				} else if ((p->getProcessorIdentifier()==TURION_ULTRA_ZM_FAMILY) ||
						(p->getProcessorIdentifier()==TURION_X2_RM_FAMILY) ||
						(p->getProcessorIdentifier()==ATHLON_X2_QL_FAMILY) ||
						(p->getProcessorIdentifier()==SEMPRON_SI_FAMILY)) {

					print_stat (p,ps,"nbvoltage",nbvoltage);
					p->setNBVid (p->convertVcoretoVID(nbvoltage));
					if (p->convertVIDtoVcore(p->getNBVid())!=nbvoltage) printf (" (rounded to %0.4fV)",p->convertVIDtoVcore(p->getNBVid()));
				}

			} else {
	
				printf ("vcore: expecting a valid float\n");

			}

		} else if (strcmp(currentCommand, "fid") == 0) {

			argcOffset++;

			if (!requireFloat(argc, argv, argcOffset, &fid)) {

				argcOffset++;
				print_stat(p, ps, "FID", fid);
				p->setFID(ps, fid);
				if (p->getFID(ps)!=fid) printf (" (rounded to %0.0f)", p->getFID(ps));

			} else {

				printf("fid: expecting a valid float\n");

			}

		} else if (strcmp(currentCommand, "did") == 0) {

			argcOffset++;

			if (!requireFloat(argc, argv, argcOffset, &did)) {

				argcOffset++;
				print_stat(p, ps, "DID", did);
				p->setDID(ps, did);
				if (p->getDID(ps)!=did) printf (" (rounded to %0.2f)", p->getDID(ps));

			} else {

				printf("did: expecting a valid float\n");

			}

		} else if (strcmp(currentCommand, "vid") == 0) {

			argcOffset++;

			if (!requireUnsignedInteger(argc, argv, argcOffset, &vid)) {

				argcOffset++;
				print_stat(p, ps, "VID", vid);
				p->setVID(ps, vid);
				if (p->getVID(ps)!=vid) printf (" (rounded to %d)", p->getVID(ps));

			} else {

				printf("vid: expecting a valid float\n");

			}

		} else {
			printf ("Unexpected command: %s",currentCommand);
			argcOffset++;
		}

		printf ("\n");

	} while (true);

	return argcOffset;

}

int main (int argc,const char **argv) {

	int argvStep;
	unsigned int currentNode;
	unsigned int currentCore;

	Processor *processor;
	PState ps(0);

	bool autoRecall=false;
	int autoRecallTimer=60;
	
	CfgManager *cfgInstance;
	int errorLine;
	
	Scaler *scaler;
	
	printf ("TurionPowerControl %s (%s)\n", _VERSION, _SOURCE_VERSION);
	printf ("Turion Power States Optimization and Control - by blackshard\n");

	if (argc<2) {
		printUsage(argv[0]);

		return 0;
	}

	if (initWinRing0()==false) {
		return false;
	}

	processor=getSupportedProcessor ();
	if (processor==NULL) {
		printf ("No supported processor detected, sorry.\n");
		return -2;
	}
	
	//Initializes currentNode and currentCore
	currentNode=processor->ALL_NODES;
	currentCore=processor->ALL_CORES;

	//Initializes the scaler based on the processor found in the system
	scaler=new Scaler (processor);
	
	for (argvStep=1;argvStep<argc;argvStep++) {

		//Reinitializes the processor object for active node and core in the system
		processor->setNode(currentNode);
		processor->setCore(currentCore);

		//printf ("Parsing argument %d %s\n",argvStep,argv[argvStep]);

		//List power states action
		if (strcmp((const char *)argv[argvStep],"-l")==0) {
			processorStatus (processor);
		}

		//Set the current operational node
		if (strcmp((const char *) argv[argvStep], "-node") == 0) {

			if ((argv[argvStep + 1] == NULL)) {
				printf("Wrong -node setting\n");
				return 1;
			}

			if (strcmp((const char *) argv[argvStep+1], "all")==0) {
				currentNode=processor->ALL_NODES;
			} else {
				unsigned int thisNode;
				thisNode=arg2i(argv, argvStep + 1);
				if ((thisNode>=0) && (thisNode<processor->getProcessorNodes()))
					currentNode=thisNode;
				else
					printf ("Node must be between 0 and %d\n", (processor->getProcessorNodes()-1));
			}

			argvStep++;
		}

		//Set the current operational core
		if (strcmp((const char *) argv[argvStep], "-core") == 0) {

			if ((argv[argvStep + 1] == NULL)) {
				printf("Wrong -node setting\n");
				return 1;
			}

			if (strcmp((const char *) argv[argvStep+1], "all")==0) {
				currentCore=processor->ALL_CORES;
			} else {
				unsigned int thisCore;
				thisCore=arg2i(argv, argvStep + 1);
				if ((thisCore>=0) && (thisCore<processor->getProcessorCores()))
					currentCore=thisCore;
				else
					printf ("Core must be between 0 and %d\n", (processor->getProcessorCores()-1));
			}

			argvStep++;
		}

		if (strcmp((const char *)argv[argvStep],"-nbvid")==0) {
			
			if (processor->getProcessorIdentifier()==PROCESSOR_10H_FAMILY) {
				if ((argv[argvStep+1]==NULL) || (argv[argvStep+2]==NULL)) {
					printf ("Wrong -nbvid option, Family 10h processor requires 2 parameters (pstate, nbvid).\n");
					return 1;
				}

				processor->setNBVid (arg2i(argv,argvStep+1),arg2i(argv,argvStep+2));
				argvStep=argvStep+2;

			} else {

				if ((argv[argvStep+1]==NULL)) {
					printf ("Wrong -nbvid option, Family 11h processor requires 1 parameter (nbdid).\n");
					return 1;
				}

				processor->setNBVid (arg2i(argv,argvStep+1));
				argvStep=argvStep+1;

			}
			
		}

		if (strcmp((const char *)argv[argvStep],"-nbdid")==0) {
			
			if ((argv[argvStep+1]==NULL) || (argv[argvStep+2]==NULL)) {
				printf ("Wrong -nbdid option.\n");
				return 1;
			}

			processor->setNBDid (arg2i(argv,argvStep+1),arg2i(argv,argvStep+2));
			argvStep=argvStep+2;
			
		}


		if (strcmp((const char *)argv[argvStep], "-nbfid") == 0) {
			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -nbfid requires a parameter (fidId).\n");
				return 1;
			}

			processor->setNBFid(arg2i(argv,argvStep+1));
			argvStep += 1;
		}


		//Enables a specified PState for current cores and current nodes
		if (strcmp((const char *)argv[argvStep],"-en")==0) {
			
			if (argv[argvStep+1]==NULL) {
				printf ("Wrong -en option\n");
				return 1;
			}
			
			ps.setPState (arg2i(argv,argvStep+1));
			processor->pStateEnable (ps.getPState());

			argvStep++;
		}

		//Disables a specified PState for current cores and current nodes
		if (strcmp((const char *)argv[argvStep],"-di")==0) {
			
			if (argv[argvStep+1]==NULL) {
				printf ("Wrong -di option\n");
				return 1;
			}
			
			ps.setPState (arg2i(argv,argvStep+1));
			processor->pStateDisable (ps.getPState());

			argvStep++;
		}

		//Set maximum PState for current nodes
		if (strcmp((const char *)argv[argvStep],"-psmax")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -psmax option\n");
				return 1;
			}
			
			ps.setPState (arg2i(argv,argvStep+1));
			processor->setMaximumPState (ps);
			argvStep++;
		}

		//Force transition to a pstate for current cores and current nodes
		if (strcmp((const char *)argv[argvStep],"-fo")==0) {
			
			if (argv[argvStep+1]==NULL) {
				printf ("Wrong -fo option\n");
				return 1;
			}
			
			ps.setPState (arg2i(argv,argvStep+1));
			processor->forcePState (ps.getPState());

			argvStep++;
		}
		
		if (strcmp((const char *)argv[argvStep],"-bst")==0)
		{
			if (argv[argvStep + 1]==NULL)
			{
				printf ("Wrong -bst option\n");
				return 1;
			}
			
			processor->setNumBoostStates(arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Show temperature table
		if (strcmp((const char *)argv[argvStep],"-temp")==0) {
			
			processorTempStatus(processor);
		
		}

		//Set vsSlamTime for current nodes
		if (strcmp((const char *)argv[argvStep],"-slamtime")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -slamtime option\n");
				return 1;
			}
			
			processor->setSlamTime (arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set vsAltVIDSlamTime for current nodes
		if (strcmp((const char *)argv[argvStep],"-altvidslamtime")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -altvidslamtime option\n");
				return 1;
			}
			
			processor->setAltVidSlamTime (arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set Ramp time for StepUpTime for current nodes
		if (strcmp((const char *)argv[argvStep],"-rampuptime")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -rampuptime option\n");
				return 1;
			}
			
			processor->setStepUpRampTime (arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set Ramp time for StepDownTime for current nodes
		if (strcmp((const char *)argv[argvStep],"-rampdowntime")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -rampdowntime option\n");
				return 1;
			}
			
			processor->setStepDownRampTime (arg2i(argv,argvStep+1));
			argvStep++;
		}
		
		if (strcmp((const char *)argv[argvStep],"-gettdp")==0)
		{
			processor->getTDP();
		}

		//Show information about per-family specifications
		if (strcmp((const char *) argv[argvStep], "-spec") == 0) {

			processor->showFamilySpecs();
		}

		//Show information about DRAM timing register
		if (strcmp((const char *) argv[argvStep], "-dram") == 0) {

			processor->showDramTimings();
		}

		//Show information about HTC registers status
		if (strcmp((const char *)argv[argvStep],"-htc")==0) {
			
			processor->showHTC();
		}
		
		//Enables HTC Features for current nodes
		if (strcmp((const char *)argv[argvStep],"-htcenable")==0) {
			
			processor->HTCEnable();
		}

		//Disables HTC Features for current nodes
		if (strcmp((const char *)argv[argvStep],"-htcdisable")==0) {
			
			processor->HTCDisable();
		}

		//Set HTC temperature limit for current nodes
		if (strcmp((const char *)argv[argvStep],"-htctemplimit")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -htctemplimit option\n");
				return 1;
			}
			
			processor->HTCsetTempLimit(arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set HTC hysteresis limit for current nodes
		if (strcmp((const char *)argv[argvStep],"-htchystlimit")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -htchystlimit option\n");
				return 1;
			}
			
			processor->HTCsetHystLimit(arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set AltVID for current nodes
		if (strcmp((const char *)argv[argvStep],"-altvid")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -altvid option\n");
				return 1;
			}
			
			processor->setAltVid(arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Show information about Hypertransport registers
		if (strcmp((const char *)argv[argvStep],"-htstatus")==0) {
			
			processor->showHTLink();
		}

		//Set Hypertransport Link frequency for current nodes
		if (strcmp((const char *)argv[argvStep],"-htset")==0) {
			
			if ((argv[argvStep+1]==NULL) || (argv[argvStep+2]==NULL)) {
				printf ("Wrong -htset option\n");
				return 1;
			}
			
			processor->setHTLinkSpeed(arg2i(argv,argvStep+1),arg2i(argv,argvStep+2));
			argvStep+=2;
		}
		
		//Enables PSI_L bit for current nodes
		if (strcmp((const char *)argv[argvStep],"-psienable")==0) {
			
			processor->setPsiEnabled (true);
		}
		
		//Disables PSI_L bit for current nodes
		if (strcmp((const char *)argv[argvStep],"-psidisable")==0) {
			
			processor->setPsiEnabled (false);
		}
		
		//Set PSI_L bit threshold for current nodes
		if (strcmp((const char *)argv[argvStep],"-psithreshold")==0) {
			
			if ((argv[argvStep+1]==NULL)) {
				printf ("Wrong -psithreshold option\n");
				return 1;
			}
			
			processor->setPsiThreshold(arg2i(argv,argvStep+1));
			argvStep++;
		}

		//Set C1E enabled on current nodes and current cores
		if (strcmp((const char *)argv[argvStep],"-c1eenable")==0) {
			
			processor->setC1EStatus(true);

		}
		
		//Set C1E disabled on current nodes and current cores
		if (strcmp((const char *)argv[argvStep],"-c1edisable")==0) {

			processor->setC1EStatus(false);
			
		}
		
		//Set Boost state to enabled for supported processors
		if (strcmp((const char *)argv[argvStep],"-boostenable")==0)
		{	
			processor->setBoost(true);
		}
		
		//Set Boost state to disabled for supported processors
		if (strcmp((const char *)argv[argvStep],"-boostdisable")==0)
		{
			processor->setBoost(false);
		}

		//Goes in temperature monitoring
		if (strcmp((const char *)argv[argvStep],"-mtemp")==0) {
			
			processorTempMonitoring(processor);
		}

		//Goes into Check Mode and controls very fastly if a transition to a wrong pstate happens
		if (strcmp((const char *)argv[argvStep],"-CM")==0)
		{
			processor->checkMode ();
		}

		//Allow cyclic parameter auto recall
		if (strcmp((const char*)argv[argvStep],"-autorecall")==0) {
		
			if (autoRecall==false) {
				autoRecall=true;
				autoRecallTimer=60;
			}
		}

		//Get general info about Performance counters
		if (strcmp((const char*)argv[argvStep],"-pcgetinfo")==0) {

			processor->perfCounterGetInfo ();

		}

		//Get Performance counter value about a specific performance counter
		if (strcmp((const char*)argv[argvStep],"-pcgetvalue")==0) {

			if (argv[argvStep+1]==NULL) {
				printf ("Wrong -pcgetvalue option\n");
				return 1;
			}

			processor->perfCounterGetValue (atoi(argv[argvStep+1]));
			argvStep++;

		}

		//Costantly monitors Performance counter value about a specific performance counter
		if (strcmp((const char*)argv[argvStep],"-pcmonitor")==0) {

			if ((argv[argvStep+1]==NULL) || (argv[argvStep+2]==NULL)) {
				printf ("Wrong -pcmonitor option\n");
				return 1;
			}

			//processor->perfCounterMonitor (atoi(argv[argvStep+1]), atoi (argv[argvStep+2]));
			argvStep+=2;

		}

		//Handle -set switch. That is a user friendly way to set up a pstate or a pstate/core
		//with frequency value and voltage value.
		if (strcmp((const char*)argv[argvStep],"-set")==0) {

			argvStep=parseSetCommand (processor,argc,argv,argvStep+1)-1;

			printf ("*** -set parsing completed\n");

		}

		//Costantly monitors CPU Usage 
		if (strcmp((const char*)argv[argvStep],"-perf-cpuusage")==0) {

			processor->perfMonitorCPUUsage ();

		}
		
		//Costantly monitors FPU Usage
		if (strcmp((const char*)argv[argvStep],"-perf-fpuusage")==0) {

			processor->perfMonitorFPUUsage ();

		}

		//Constantly monitors Data Cache Misaligned Accesses
		if (strcmp((const char*)argv[argvStep],"-perf-dcma")==0) {

			processor->perfMonitorDCMA();

		}



		//Open a configuration file
		if (strcmp((const char*)argv[argvStep],"-cfgfile")==0) {
		
			cfgInstance=new CfgManager (processor, scaler);
			
			if (cfgInstance->openCfgFile ((char *)argv[argvStep+1])) {
				printf ("Error: invalid configuration file\n");
				free (cfgInstance);
				break;
			}
			
			errorLine=cfgInstance->parseCfgFile ();
			
			if (errorLine!=0) {
				printf ("Error: invalid configuration identifier at row %d\n",errorLine);
				free (cfgInstance);
				break;
			}
		
			free (cfgInstance); 
			
			argvStep++;
			
		
		}
		
		if (strcmp((const char *)argv[argvStep],"-scaler")==0) {

			printf ("Scaler is not active in this version.\n");
			scaler->beginScaling ();

		}

		//Autorecall feature set argvStep back to 1 when it reaches end
		if ((autoRecall==true) && (argvStep==(argc-1))) {
			printf ("Autorecall activated. Timeout: %d seconds\n", autoRecallTimer);
			Sleep (autoRecallTimer*1000);
			printf ("Autorecalling...\n");
			argvStep=0;
		}


	}

	printf ("\n");

	printf ("Done.\n");

	free (processor);

	closeWinRing0 ();

	return 0;
}
