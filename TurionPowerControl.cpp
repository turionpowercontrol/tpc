#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

//This program is supposed to compile with GCC under Linux x86/x86-64 platforms
//and with Microsoft Visual C++ 2005/2008 under Windows x86/x86-64 platforms

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

#include "sysdep.h"

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
static bool requireInteger (int argc, const char **argv, int offset, int *output) {

	const char *argument;
	long value;
	char *end;

	if (offset >= argc)
		return true;

	argument = argv[offset];

	if (argument[0] == '\0')
		return true;

	value = strtol(argument, &end, 0);

	if (end[0] != '\0')
		return true;

	if (value < INT_MIN)
		return true;

	if (value > INT_MAX)
		return true;

	*output = value;

	return false;
}

//Equal as above, but with unsigned integer values
static bool requireUnsignedInteger (int argc, const char **argv, int offset, unsigned int *output) {

	int value;

	if (requireInteger(argc, argv, offset, &value))
		return true;

	if (value < 0)
		return true;

	*output = value;

	return false;
}


//Function used by parseSetCommand to obtain a valid float.
//The float is put in output pointer if the function returns false (false
//means no error)
//Instead, if there is an error (no valid float, a string where there 
//should be a value, ...) , the function returns true.
bool requireFloat (int argc, const char **argv, int offset, float *output) {

	const char *argument;
	double value;
	char *end;

	if (offset >= argc)
		return true;

	argument = argv[offset];

	if (argument[0] == '\0')
		return true;

	value = strtod(argument, &end);

	if (end[0] != '\0')
		return true;

	*output = value;

	return false;
}

//Simple method used by parseSetCommand to show some useful and tidy informations.
#define PRINT_STAT_FLAG_NONE	0x00
#define PRINT_STAT_FLAG_CORE	0x01
#define PRINT_STAT_FLAG_NODE	0x02
#define PRINT_STAT_FLAG_PSTATE	0x04
#define PRINT_STAT_FLAG_ALL	0x07
void print_stat(Processor *p, PState ps, const char *what, float value, int flags)
{
		if (flags & PRINT_STAT_FLAG_NODE) {
			if (p->getNode() == p->ALL_NODES)
				printf("All nodes ");
			else
				printf("Node: %d ", p->getNode());
		}
		if (flags & PRINT_STAT_FLAG_CORE) {
			if (p->getCore() == p->ALL_CORES)
				printf("all cores ");
			else
				printf("core: %d ", p->getCore());
		}
		if (flags & PRINT_STAT_FLAG_PSTATE) {
			printf("pstate: %d ", ps.getPState());
		}
		printf("-- set %s to %0.4f", what, value);
}

#define PARSE_WRONG_FORMAT		-1 /* known argument but used incorrectly */
#define PARSE_UNEXPECTED_ARGUMENT	-2 /* unexpected argument -- relieve control*/
#define PARSE_UNKNOWN_ARGUMENT		-3 /* unknown argument */
static int parseSingleSetSubcommand(Processor *p, int argc, const char **argv, int argcOffset, int consume_dash, PState *ps)
{
	const char *currentCommand = argv[argcOffset];

	assert(consume_dash == 1 || consume_dash == 0);
	if (consume_dash) {
		if (currentCommand[0] != '-') {
			return PARSE_UNEXPECTED_ARGUMENT;
		}
		currentCommand++;
	} else {
		if (currentCommand[0] == '-') {
			return PARSE_UNEXPECTED_ARGUMENT;
		}
	}

	/*
	 * Following section will alter operating pstate. By default, pstate 0 is set
	 */
	if (strcmp (currentCommand, "pstate") == 0 ||
		strcmp (currentCommand, "ps") == 0) {

		unsigned int pstate;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireUnsignedInteger(argc, argv, argcOffset, &pstate)) {
			printf("ERROR: invalid pstate -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		if (pstate >= p->getPowerStates()) {
			printf("ERROR: pstate must be in 0-%u range\n", p->getPowerStates() - 1);
			return PARSE_WRONG_FORMAT;
		}
		ps->setPState(pstate);
		argcOffset++;
		return argcOffset;
	}

	/*
	 * Following section will alter operating core. Default setting is core=all
	 */
	if (strcmp(currentCommand, "core") == 0) {

		unsigned int core;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (strcmp(argv[argcOffset], "all") != 0) {
			if (requireUnsignedInteger(argc, argv, argcOffset, &core)) {
				printf("ERROR: invalid core -- %s\n", argv[argcOffset]);
				return PARSE_WRONG_FORMAT;
			}
			if (core >= p->getProcessorCores()) {
				printf("ERROR: core must be in 0-%u range\n", p->getProcessorCores() - 1);
				return PARSE_WRONG_FORMAT;
			}
			p->setCore(core);
		} else {
			p->setCore(p->ALL_CORES);
		}
		argcOffset++;
		return argcOffset;
	}

	/*
	 * Following section will alter operating node. Default setting is node=all
	 */
	if (strcmp(currentCommand, "node") == 0) {

		unsigned int node;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (strcmp(argv[argcOffset], "all") != 0) {
			if (requireUnsignedInteger(argc, argv, argcOffset, &node)) {
				printf("ERROR: invalid node -- %s\n", argv[argcOffset]);
				return PARSE_WRONG_FORMAT;
			}
			if (node >= p->getProcessorNodes()) {
				printf("ERROR: node must be in 0-%u range\n", p->getProcessorNodes() - 1);
				return PARSE_WRONG_FORMAT;
			}
			p->setNode(node);
		} else {
			p->setNode(p->ALL_NODES);
		}
		argcOffset++;
		return argcOffset;
	}

	/*
	 * Following section will set a new frequency for selected pstate/core/node
	 */
	if (strcmp(currentCommand, "freq") == 0 || 
		strcmp(currentCommand, "f") == 0 || 
		strcmp(currentCommand, "frequency") == 0) {

		unsigned int frequency;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireUnsignedInteger(argc, argv, argcOffset, &frequency)) {
			printf("ERROR: invalid frequency -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		p->setFrequency(*ps, frequency);
		print_stat(p, *ps, "frequency", frequency, PRINT_STAT_FLAG_ALL);
		if (p->getFrequency(*ps) != frequency)
			printf(" (actual: %d)", p->getFrequency(*ps));
		printf("\n");
		argcOffset++;
		return argcOffset;
	}

	/*
	 * Following section will set a new core voltage for operating pstate/core/node
	 */
	if (strcmp(currentCommand, "vcore") == 0 || 
		strcmp(currentCommand, "vc") == 0 ||
		strcmp(currentCommand, "voltage") == 0) {

		float voltage;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireFloat(argc, argv, argcOffset, &voltage)) {
			printf("ERROR: invalid vcore -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		p->setVCore(*ps, voltage);
		print_stat(p, *ps, "core voltage", voltage, PRINT_STAT_FLAG_ALL);
		if (p->getVCore(*ps) != voltage)
			printf(" (actual: %0.4fV)", p->getVCore(*ps));
		printf("\n");
		argcOffset++;
		return argcOffset;
	}

	/*
	 * Following section will set a new northbirdge voltage for operating
	 * core on operating node
	 */
	if (strcmp(currentCommand, "nbvoltage") == 0 || 
		strcmp(currentCommand, "nbv") == 0 ||
		strcmp(currentCommand, "nbvolt") == 0) {

		float nbvoltage;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireFloat(argc, argv, argcOffset, &nbvoltage)) {
			printf("ERROR: invald nbvoltage -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}

		//Since family 10h and family 11h differs on northbridge voltage handling, we have to make a difference
		//here. On family 10h changing northbridge voltage changes just to the pstate the user is manipulating
		//instead on family 11h it changes the northbridge voltage independently of the pstate the user is
		//manipulating
		if (p->getProcessorIdentifier() == PROCESSOR_10H_FAMILY) {

			p->setNBVid(*ps, p->convertVcoretoVID(nbvoltage));
			print_stat(p, *ps, "nbvoltage", nbvoltage, PRINT_STAT_FLAG_NODE | PRINT_STAT_FLAG_PSTATE);
			if (p->convertVIDtoVcore(p->getNBVid(*ps)) != nbvoltage)
				printf(" (actual: %0.4fV)", p->convertVIDtoVcore(p->getNBVid(*ps)));
			printf("\n");
			argcOffset++;
			return argcOffset;
		}

		if (p->getProcessorIdentifier() == TURION_ULTRA_ZM_FAMILY ||
			p->getProcessorIdentifier() == TURION_X2_RM_FAMILY ||
			p->getProcessorIdentifier() == ATHLON_X2_QL_FAMILY ||
			p->getProcessorIdentifier() == SEMPRON_SI_FAMILY) {

			p->setNBVid(p->convertVcoretoVID(nbvoltage));
			print_stat(p, *ps, "nbvoltage", nbvoltage, PRINT_STAT_FLAG_NODE);
			if (p->convertVIDtoVcore(p->getNBVid()) != nbvoltage)
				printf (" (actual: %0.4fV)", p->convertVIDtoVcore(p->getNBVid()));
			printf("\n");
			argcOffset++;
			return argcOffset;
		}
		printf("ERROR: %s -- not supported\n", currentCommand - consume_dash);
		return PARSE_WRONG_FORMAT;
	}

	if (strcmp(currentCommand, "fid") == 0) {

		float fid;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireFloat(argc, argv, argcOffset, &fid)) {
			printf("ERROR: invald fid -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		p->setFID(*ps, fid);
		print_stat(p, *ps, "FID", fid, PRINT_STAT_FLAG_ALL);
		if (p->getFID(*ps) != fid)
			printf (" (actual: %0.0f)", p->getFID(*ps));
		printf("\n");
		argcOffset++;
		return argcOffset;
	}

	if (strcmp(currentCommand, "did") == 0) {

		float did;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireFloat(argc, argv, argcOffset, &did)) {
			printf("ERROR: invald did -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		p->setDID(*ps, did);
		print_stat(p, *ps, "DID", did, PRINT_STAT_FLAG_ALL);
		if (p->getDID(*ps) != did)
			printf (" (actual: %0.2f)", p->getDID(*ps));
		printf("\n");
		argcOffset++;
		return argcOffset;
	}

	if (strcmp(currentCommand, "vid") == 0) {

		unsigned int vid;

		argcOffset++;

		if (argv[argcOffset] == NULL) {
			printf("ERROR: %s requires an argument\n", currentCommand - consume_dash);
			return PARSE_WRONG_FORMAT;
		}
		if (requireUnsignedInteger(argc, argv, argcOffset, &vid)) {
			printf("ERROR: invalid frequency -- %s\n", argv[argcOffset]);
			return PARSE_WRONG_FORMAT;
		}
		p->setVID(*ps, vid);
		print_stat(p, *ps, "VID", vid, PRINT_STAT_FLAG_ALL);
		if (p->getVID(*ps) != vid)
			printf (" (actual: %d)", p->getVID(*ps));
		printf("\n");
		argcOffset++;
		return argcOffset;
	}

	return PARSE_UNKNOWN_ARGUMENT;
}

//This procedure parse the -set switch
int parseSetCommand (Processor *p, int argc, const char **argv, int argcOffset) {

	PState ps(0);
	const char *currentCommand;
	int rv;

	p->setCore(p->ALL_CORES);
	p->setNode(p->ALL_NODES);

	do {

		//If we exceed the commands, get out of the loop
		if (argcOffset>=argc) break;

		currentCommand=argv[argcOffset];

		rv = parseSingleSetSubcommand(p, argc, argv, argcOffset, 0, &ps);
		if (rv == PARSE_UNKNOWN_ARGUMENT) {
			printf("ERROR: unknown set sub-command -- %s\n", currentCommand);
			return PARSE_WRONG_FORMAT;
		}
		if (rv == PARSE_WRONG_FORMAT) {
			return PARSE_WRONG_FORMAT;
		}
		if (rv == PARSE_UNEXPECTED_ARGUMENT) {
			break;
		}
		argcOffset = rv;
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

	int rv;
	int parsed = 0;
	int parsed_set = 0;

	printf ("TurionPowerControl %s (%s)\n", _VERSION, _SOURCE_VERSION);
	printf ("Turion Power States Optimization and Control - by blackshard\n\n");

	if (argc<2) {
		printUsage(argv[0]);

		return 0;
	}

	if (initializeCore() == false) {
		return -1;
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
	
	for (argvStep = 1; ; argvStep++) {

		if (argvStep == argc) {
			if (!autoRecall)
				break;
			//Autorecall feature set argvStep back to 1 when it reaches end
			printf("Autorecall activated. Timeout: %d seconds\n", autoRecallTimer);
			Sleep(autoRecallTimer * 1000);
			printf("Autorecalling...\n");
			argvStep = 1;
		}

		//Reinitializes the processor object for active node and core in the system
		processor->setNode(currentNode);
		processor->setCore(currentCore);

		//printf ("Parsing argument %d %s\n",argvStep,argv[argvStep]);

		if (parsed_set) {
			printf("ERROR: -set can only be used exclusively\n");
			break;
		}
		parsed++;

		//List power states action
		if (strcmp(argv[argvStep], "-l") == 0) {

			processorStatus (processor);
			continue;
		}

		//Set the current operational node
		if (strcmp(argv[argvStep], "-node") == 0) {

			unsigned int thisNode;
			const char *arg = argv[argvStep + 1];

			if (arg == NULL) {
				printf("-node requires an argument\n");
				break;
			}
			if (strcmp(arg, "all") != 0) {
				if (requireUnsignedInteger(argc, argv, argvStep + 1, &thisNode)) {
					printf("ERROR: invalid -node -- %s\n", arg);
					break;
				}
				if (thisNode >= processor->getProcessorNodes()) {
					printf("ERROR: node must be in 0-%u range\n", processor->getProcessorNodes() - 1);
					break;
				}
				currentNode = thisNode;
			} else {
				currentNode = processor->ALL_NODES;
			}
			argvStep++;
			continue;
		}

		//Set the current operational core
		if (strcmp(argv[argvStep], "-core") == 0) {

			unsigned int thisCore;
			const char *arg = argv[argvStep + 1];

			if (arg == NULL) {
				printf("ERROR: -core requires an argument\n");
				break;
			}
			if (strcmp(arg, "all") != 0) {
				if (requireUnsignedInteger(argc, argv, argvStep + 1, &thisCore)) {
					printf("ERROR: invalid -core -- %s\n", arg);
					break;
				}
				if (thisCore >= processor->getProcessorCores()) {
					printf("ERROR: core must be in 0-%u range\n", processor->getProcessorCores() - 1);
					break;
				}
				currentCore = thisCore;
			} else {
				currentCore = processor->ALL_CORES;
			}
			argvStep++;
			continue;
		}

		rv = parseSingleSetSubcommand(processor, argc, argv, argvStep, 1, &ps);
		if (rv >= 0) {
			printf("\n");
			argvStep = rv - 1;
			continue;
		}
		if (rv == PARSE_WRONG_FORMAT) {
			break;
		}

		if (strcmp(argv[argvStep], "-nbvid") == 0) {
			unsigned int nbvid;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -nbvid requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &nbvid)) { 
				printf("ERROR: invalid NBVid -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (processor->getProcessorIdentifier() == PROCESSOR_10H_FAMILY) {
				processor->setNBVid (ps, nbvid);
				print_stat(processor, ps, "nbvid", nbvid, PRINT_STAT_FLAG_NODE | PRINT_STAT_FLAG_PSTATE);
				if (processor->getNBVid(ps) != nbvid)
					printf(" (actual: %d)", processor->getNBVid(ps));
				printf("\n\n");
			} else {
				processor->setNBVid (nbvid);
				print_stat(processor, ps, "nbvid", nbvid, PRINT_STAT_FLAG_NODE);
				if (processor->getNBVid() != nbvid)
					printf(" (actual: %d)", processor->getNBVid());
				printf("\n\n");
			}
			argvStep = argvStep + 1;
			continue;
		}

		if (strcmp(argv[argvStep], "-nbdid") == 0) {

			if (processor->getProcessorIdentifier() == PROCESSOR_10H_FAMILY) {
				unsigned int nbdid;

				if (argv[argvStep + 1] == NULL) {
					printf("ERROR: -nbdid requires an argument\n");
					break;
				}
				if (requireUnsignedInteger(argc, argv, argvStep + 1, &nbdid)) { 
					printf("ERROR: invalid NBDid -- %s\n", argv[argvStep + 1]);
					break;
				}
				processor->setNBDid (ps, nbdid);
				print_stat(processor, ps, "nbdid", nbdid, PRINT_STAT_FLAG_NODE | PRINT_STAT_FLAG_PSTATE);
				if (processor->getNBDid(ps) != nbdid)
					printf(" (actual: %d)", processor->getNBDid(ps));
				printf("\n\n");
				argvStep = argvStep + 1;
			} else {
				printf("ERROR: -nbdid is only supported on family 10h processors\n");
				break;
			}
			continue;
		}

		if (strcmp(argv[argvStep], "-nbfid") == 0) {

			unsigned int nbfid;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -nbfid requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &nbfid)) { 
				printf("ERROR: invalid NBFid -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setNBFid(nbfid);
			print_stat(processor, ps, "nbfid", nbfid, PRINT_STAT_FLAG_NODE);
			if (processor->getNBFid() != nbfid)
				printf(" (actual: %d)", processor->getNBFid());
			printf("\n\n");
			argvStep += 1;
			continue;
		}

		/*
		 * Following section will set a new frequency for selected pstate/core/node
		 */
		if (strcmp(argv[argvStep], "-nbfreq") == 0 || 
			strcmp(argv[argvStep], "-nbf") == 0 || 
			strcmp(argv[argvStep], "-nbfrequency") == 0) {

			unsigned int nbfrequency;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: %s requires an argument\n", argv[argvStep + 1]);
				return -1;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &nbfrequency)) {
				printf("ERROR: invalid nbfrequency -- %s\n", argv[argvStep + 1]);
				return -1;
			}
			processor->setNBFrequency(ps, nbfrequency);
			print_stat(processor, ps, "nbfrequency", nbfrequency, PRINT_STAT_FLAG_NODE | PRINT_STAT_FLAG_PSTATE);
			if (processor->getNBFrequency(ps) != nbfrequency)
				printf(" (actual: %d)", processor->getNBFrequency(ps));
			printf("\n\n");
			argvStep++;
			continue;
		}

		//Enables a specified PState for current cores and current nodes
		if (strcmp(argv[argvStep], "-en") == 0) {

			unsigned int pstate;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -en requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &pstate)) { 
				printf("ERROR: -en: invalid P-state -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (pstate >= processor->getPowerStates()) {
				printf("ERROR: -en: P-state must be in 0-%u range\n", processor->getPowerStates() - 1);
				break;
			}
			processor->pStateEnable(pstate);
			argvStep++;
			continue;
		}

		//Disables a specified PState for current cores and current nodes
		if (strcmp(argv[argvStep], "-di") == 0) {

			unsigned int pstate;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -di requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &pstate)) { 
				printf("ERROR: -di: invalid P-state -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (pstate >= processor->getPowerStates()) {
				printf("ERROR: -di: P-state must be in 0-%u range\n", processor->getPowerStates() - 1);
				break;
			}
			processor->pStateDisable(pstate);
			argvStep++;
			continue;
		}

		//Set maximum PState for current nodes
		if (strcmp(argv[argvStep], "-psmax") == 0) {

			unsigned int pstate;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -psmax requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &pstate)) { 
				printf("ERROR: invalid P-state -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (pstate >= processor->getPowerStates()) {
				printf("ERROR: P-state must be in 0-%u range\n", processor->getPowerStates() - 1);
				break;
			}
			processor->setMaximumPState(pstate);
			argvStep++;
			continue;
		}

		//Force transition to a pstate for current cores and current nodes
		if (strcmp(argv[argvStep], "-fo") == 0) {

			unsigned int pstate;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -fo requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &pstate)) { 
				printf("ERROR: -fo: invalid P-state -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (pstate >= processor->getPowerStates() - processor->getBoostStates()) {
				printf("ERROR: -fo: P-state (software P-state) must be in 0-%u range\n", processor->getPowerStates() - processor->getBoostStates() - 1);
				break;
			}
			processor->forcePState(pstate);
			argvStep++;
			continue;
		}

		if (strcmp(argv[argvStep], "-bst") == 0) {

			unsigned int numBoostStates;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -bst requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &numBoostStates)) { 
				printf("ERROR: invalid numBoostStates -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setNumBoostStates(numBoostStates);
			argvStep++;
			continue;
		}

		//Show temperature table
		if (strcmp(argv[argvStep], "-temp") == 0) {

			processorTempStatus(processor);
			continue;
		}

		//Set vsSlamTime for current nodes
		if (strcmp(argv[argvStep], "-slamtime") == 0) {

			unsigned int slamtime;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -slamtime requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &slamtime)) { 
				printf("ERROR: invalid slamtime -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setSlamTime(slamtime);
			argvStep++;
			continue;
		}

		//Set vsAltVIDSlamTime for current nodes
		if (strcmp(argv[argvStep], "-altvidslamtime") == 0) {

			unsigned int altvidslamtime;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -altvidslamtime requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &altvidslamtime)) { 
				printf("ERROR: invalid altvidslamtime -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setAltVidSlamTime(altvidslamtime);
			argvStep++;
			continue;
		}

		//Set Ramp time for StepUpTime for current nodes
		if (strcmp(argv[argvStep], "-rampuptime") == 0) {

			unsigned int rampuptime;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -rampuptime requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &rampuptime)) { 
				printf("ERROR: invalid rampuptime -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setStepUpRampTime(rampuptime);
			argvStep++;
			continue;
		}

		//Set Ramp time for StepDownTime for current nodes
		if (strcmp(argv[argvStep], "-rampdowntime") == 0) {

			unsigned int rampdowntime;

			if (argv[argvStep + 1] == NULL) {
				printf ("ERROR: -rampdowntime requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &rampdowntime)) { 
				printf("ERROR: invalid rampdowntime -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setStepDownRampTime(rampdowntime);
			argvStep++;
			continue;
		}

		if (strcmp(argv[argvStep], "-gettdp") == 0) {

			processor->getTDP();
			continue;
		}

		//Show information about per-family specifications
		if (strcmp(argv[argvStep], "-spec") == 0) {

			processor->showFamilySpecs();
			continue;
		}

		//Show information about DRAM timing register
		if (strcmp(argv[argvStep], "-dram") == 0) {

			processor->showDramTimings();
			continue;
		}

		//Show information about HTC registers status
		if (strcmp(argv[argvStep], "-htc") == 0) {

			processor->showHTC();
			continue;
		}
		
		//Enables HTC Features for current nodes
		if (strcmp(argv[argvStep], "-htcenable") == 0) {

			processor->HTCEnable();
			continue;
		}

		//Disables HTC Features for current nodes
		if (strcmp(argv[argvStep], "-htcdisable") == 0) {

			processor->HTCDisable();
			continue;
		}

		//Set HTC temperature limit for current nodes
		if (strcmp(argv[argvStep], "-htctemplimit") == 0) {

			unsigned int htctemplimit;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: -htctemplimit requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &htctemplimit)) { 
				printf("ERROR: invalid htctemplimit -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->HTCsetTempLimit(htctemplimit);
			argvStep++;
			continue;
		}

		//Set HTC hysteresis limit for current nodes
		if (strcmp(argv[argvStep], "-htchystlimit") == 0) {

			unsigned int htchystlimit;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: -htchystlimit requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &htchystlimit)) { 
				printf("ERROR: invalid htchystlimit -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->HTCsetHystLimit(htchystlimit);
			argvStep++;
			continue;
		}

		//Set AltVID for current nodes
		if (strcmp(argv[argvStep], "-altvid") == 0) {

			unsigned int altvid;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: -altvid requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &altvid)) { 
				printf("ERROR: invalid altvid -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setAltVid(altvid);
			argvStep++;
			continue;
		}

		//Show information about Hypertransport registers
		if (strcmp(argv[argvStep], "-htstatus") == 0) {

			processor->showHTLink();
			continue;
		}

		//Set Hypertransport Link frequency for current nodes
		if (strcmp(argv[argvStep], "-htset") == 0) {

			unsigned int reg;
			unsigned int value;

			if ((argv[argvStep + 1] == NULL) || (argv[argvStep + 2] == NULL)) {
				printf("ERROR: -htset requires two arguments (register, value)\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &reg)) { 
				printf("ERROR: invalid register -- %s\n", argv[argvStep + 1]);
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 2, &value)) { 
				printf("ERROR: invalid value -- %s\n", argv[argvStep + 2]);
				break;
			}
			processor->setHTLinkSpeed(reg, value);
			argvStep += 2;
			continue;
		}

		//Enables PSI_L bit for current nodes
		if (strcmp(argv[argvStep], "-psienable") == 0) {

			processor->setPsiEnabled (true);
			continue;
		}

		//Disables PSI_L bit for current nodes
		if (strcmp(argv[argvStep], "-psidisable") == 0) {

			processor->setPsiEnabled (false);
			continue;
		}

		//Set PSI_L bit threshold for current nodes
		if (strcmp(argv[argvStep], "-psithreshold") == 0) {

			unsigned int psithreshold;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: -psithreshold requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &psithreshold)) { 
				printf("ERROR: invalid psithreshold -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->setPsiThreshold(psithreshold);
			argvStep++;
			continue;
		}

		//Set C1E enabled on current nodes and current cores
		if (strcmp(argv[argvStep], "-c1eenable") == 0) {

			processor->setC1EStatus(true);
			continue;
		}

		//Set C1E disabled on current nodes and current cores
		if (strcmp(argv[argvStep], "-c1edisable") == 0) {

			processor->setC1EStatus(false);
			continue;
		}

		//Set Boost state to enabled for supported processors
		if (strcmp(argv[argvStep], "-boostenable") == 0) {	

			processor->setBoost(true);
			continue;
		}

		//Set Boost state to disabled for supported processors
		if (strcmp(argv[argvStep], "-boostdisable") == 0) {

			processor->setBoost(false);
			continue;
		}

		//Goes in temperature monitoring
		if (strcmp(argv[argvStep], "-mtemp") == 0) {

			processorTempMonitoring(processor);
			continue;
		}

		//Goes into Check Mode and controls very fastly if a transition to a wrong pstate happens
		if (strcmp(argv[argvStep], "-CM") == 0) {

			processor->checkMode();
			continue;
		}

		//Allow cyclic parameter auto recall
		if (strcmp(argv[argvStep], "-autorecall") == 0) {

			if (autoRecall == false) {
				autoRecall = true;
				autoRecallTimer = 60;
			}
			continue;
		}

		//Get general info about Performance counters
		if (strcmp(argv[argvStep], "-pcgetinfo") == 0) {

			processor->perfCounterGetInfo();
			continue;
		}

		//Get Performance counter value about a specific performance counter
		if (strcmp(argv[argvStep], "-pcgetvalue") == 0) {

			unsigned int counter;

			if (argv[argvStep + 1] == NULL) {
				printf("ERROR: -pcgetvalue requires an argument\n");
				break;
			}
			if (requireUnsignedInteger(argc, argv, argvStep + 1, &counter)) { 
				printf("ERROR: invalid counter -- %s\n", argv[argvStep + 1]);
				break;
			}
			processor->perfCounterGetValue(counter);
			argvStep++;
			continue;
		}

		//Costantly monitors Performance counter value about a specific performance counter
		if (strcmp(argv[argvStep], "-pcmonitor") == 0) {
			printf("ERROR: -pcmonitor is currently not implemented\n");
			break;
		}

		//Handle -set switch. That is a user friendly way to set up a pstate or a pstate/core
		//with frequency value and voltage value.
		if (strcmp(argv[argvStep], "-set") == 0) {

			parsed_set++;
			
			if (parsed - parsed_set) {
				printf("ERROR: -set can only be used exclusively\n");
				break;
			}
			printf("WARNING: -set is deprecated and will be removed in future versions.\n");
			printf("          Please consider using standalone versions of your sub-commands.\n");
			printf("          Consult the documentation for details.\n\n");
			if ((argvStep = parseSetCommand(processor, argc, argv, argvStep + 1)) == PARSE_WRONG_FORMAT)
				break;

			printf ("*** -set parsing completed\n");
			argvStep--;
			continue;
		}

		//Costantly monitors CPU Usage 
		if (strcmp(argv[argvStep], "-perf-cpuusage") == 0) {

			processor->perfMonitorCPUUsage ();
			continue;
		}

		//Costantly monitors FPU Usage
		if (strcmp(argv[argvStep], "-perf-fpuusage") == 0) {

			processor->perfMonitorFPUUsage ();
			continue;
		}

		//Constantly monitors Data Cache Misaligned Accesses
		if (strcmp(argv[argvStep], "-perf-dcma") == 0) {

			processor->perfMonitorDCMA();
			continue;
		}



		//Open a configuration file
		if (strcmp(argv[argvStep], "-cfgfile") == 0) {

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
			continue;
		}

		if (strcmp(argv[argvStep], "-scaler") == 0) {

			printf ("Scaler is not active in this version.\n");
			scaler->beginScaling ();
			continue;
		}

		printf("ERROR: invalid argument -- %s\n", argv[argvStep]);
		break;
	}

	if (argvStep == argc) {
		printf ("\n");
		printf ("Done.\n");
	}

	free (processor);

	deinitializeCore();

	return 0;
}
