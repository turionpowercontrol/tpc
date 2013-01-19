#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	#define uint64_t unsigned long long int
	#include <windows.h>
	#include "OlsApi.h"
#endif

#ifdef __linux
	#include "cpuPrimitives.h"
	#include <string.h>
#endif

#include "Processor.h"


PState::PState (DWORD ps) {
	pstate=ps;
}

DWORD PState::getPState () {
	return pstate;
}

void PState::setPState (DWORD ps) {
	pstate=ps;
}

void Processor::setCore (DWORD core) {

	if (!isValidCore(core)) return;

	selectedCore=core;

}

void Processor::setNode (DWORD node) {

	if (!isValidNode(node)) return;

	selectedNode=node;

}

DWORD Processor::getCore () {

	return selectedCore;

}

DWORD Processor::getNode () {

	return selectedNode;

}

/*
 * getMask - Gets a processor bitmask based on selectedCore and selectedNode
 * This is useful to use MSRObject class, since it involves
 * per-core bits. This bitmask is 64-bit wide on 64-bit systems
 * and 32-bit wide on 32-bit systems
 * If selectedCore is equal to static constant ALL_CORES and selectedNode
 * is a single node, the returned mask will have all the bits for those
 * cores of that precise node set.
 * If selectedCore is ALL_CORES and selectedNode is ALL_NODES, all bits are
 * set for all cores of all cpus present in the system
 * If selectedNode is ALL_NODES and selectedCore is a single value, then
 * the mask returned will set the selected core bit for each node in the system.
 */
PROCESSORMASK Processor::getMask (DWORD core, DWORD node)
{
	PROCESSORMASK mask;

	//In the case we are pointing to a specific core on a specific node, this is
	//the right formula to get the mask for a single cpu.
	if ((core!=ALL_CORES) && (node!=ALL_NODES))
		return (PROCESSORMASK)1<<((node*processorCores)+core);


	//If core is set to ALL_CORES and node is free we set the mask
	//to specify all the cores of one specific node
	if ((core==ALL_CORES) && (node!=ALL_NODES))
	{
		mask = 1;
		mask <<= processorCores;
		mask -= 1;
		mask <<= processorCores * node;

		return mask;
	}

	//If core is free and node is set to ALL_NODES, we set
	//the mask to cover that specific core of all the nodes
	if ((core!=ALL_CORES) && (node==ALL_NODES))
	{
		unsigned int offset;

		mask=0;
		for (offset=core;offset<(processorCores*processorNodes);offset+=processorCores)
			mask|=(PROCESSORMASK)1<<offset;

		return mask;
	}

	//If core is set to ALL_NODES and node is set to ALL_NODES,
	//we set the mask as a whole sequence of true bits, except for
	//exceeding bits
	if ((core==ALL_CORES) && (node==ALL_NODES))
	{
		mask = -1;
		mask >>= (64 - processorCores * processorNodes);

		return mask;
	}

	return 0;

}

PROCESSORMASK Processor::getMask () {

	return getMask (selectedCore, selectedNode);

}

/*
 * getNodeMask - Gets a bitmask based on selectedNode nodes.
 * This bitmask is useful for PCIRegObject class and is
 * always 32-bit wide.
 * If selectedNode is equal to ALL_NODES static constant,
 * the bitmask returned by this function will set bits for only
 * active and present nodes.
 */
DWORD Processor::getNodeMask(DWORD node)
{
	DWORD mask;

	if (node == ALL_NODES)
	{
		mask = 1;
		mask <<= processorNodes;
		mask -= 1;

		return mask;
	}

	mask = (DWORD) 1 << (node);
	return mask;

}

DWORD Processor::getNodeMask ()
{
	return getNodeMask (selectedNode);
}

//Return true if core is in the range, else shows an error message and return false
bool Processor::isValidCore (DWORD core) {

	if (core==ALL_CORES) return true;

	if (core>=0 && core<processorCores) return true; else {
		printf ("Wrong core. Allowed range: 0-%d\n", (processorCores-1));
		return false;
	}
}

//Return false if node is in the range, else shows an error message and return false
bool Processor::isValidNode (DWORD node) {

	if (node==ALL_NODES) return true;

	if (node>=0 && node<processorNodes) return true; else {
		printf ("Wrong node. Allowed range: 0-%d\n", (processorNodes-1));
		return false;
	}

}


/****** PUBLIC METHODS ********/

bool Processor::isProcessorSupported() {
	return false;
}

void Processor::showFamilySpecs() {
	printf("\n\t(No detailed specifications available for this processor family)\n");
	return;
}

void Processor::showDramTimings() {
	printf("\n\t(No detailed DRAM Timings available for this processor family)\n");
	return;
}

void Processor::showHTLink() {
	printf ("\n\t(No detailed Hypertransport Link informations available for this processor family)\n");
	return;
}

void Processor::showHTC() {
	printf ("\n\t(No detailed Hardware Thermal Control informations available for this processor family)\n");
	return;
}

float Processor::convertVIDtoVcore (DWORD curVid) {
	return 0;
}

DWORD Processor::convertVcoretoVID (float vcore) {
	return 0;
}

DWORD Processor::convertFDtoFreq (DWORD curFid, DWORD curDid) {
	return 0;
}

void Processor::convertFreqtoFD(DWORD, int *, int *) {
	return;
}

void Processor::setProcessorStrId (const char *strId) {
	if (strlen(strId) > sizeof(processorStrId) - 1)
		printf ("Warning: processor string Id Exceeds %d bytes!\n", sizeof(processorStrId) - 1);
	strncpy(processorStrId, strId, sizeof(processorStrId));
	processorStrId[sizeof(processorStrId) - 1] = '\0';
}

void Processor::setPowerStates (DWORD pstates) {
	powerStates=pstates;
}

void Processor::setProcessorCores (DWORD pcores) {
	processorCores=pcores;
}

void Processor::setProcessorIdentifier (DWORD pId) {
	processorIdentifier=pId;
}

void Processor::setProcessorNodes (DWORD nodes) {
	processorNodes=nodes;
}

DWORD Processor::HTLinkToFreq (DWORD reg) {

	switch (reg) {
	case 0x00:
		return 200;
	case 0x02:
		return 400;
	case 0x04:
		return 600;
	case 0x05:
		return 800;
	case 0x06:
		return 1000;
	case 0x07:
		return 1200;
	case 0x08:
		return 1400;
	case 0x09:
		return 1600;
	case 0x0A:
		return 1800;
	case 0x0B:
		return 2000;
	case 0x0C:
		return 2200;
	case 0x0D:
		return 2400;
	case 0x0E:
		return 2600;
	case 0x11:
		return 2800;
	case 0x12:
		return 3000;
	case 0x13:
		return 3200;
	default:
		return 0;
	}
}

void Processor::setSpecFamilyBase (int familyBase) {
	this->familyBase=familyBase;
}

void Processor::setSpecModel (int model) {
	this->model=model;
}

void Processor::setSpecStepping (int stepping) {
	this->stepping=stepping;
}

void Processor::setSpecFamilyExtended (int familyExtended) {
	this->familyExtended=familyExtended;
}

void Processor::setSpecModelExtended (int modelExtended) {
	this->modelExtended=modelExtended;
}

void Processor::setSpecBrandId (int brandId) {
	this->brandId=brandId;
}

void Processor::setSpecProcessorModel (int processorModel) {
	this->processorModel=processorModel;
}

void Processor::setSpecString1 (int string1) {
	this->string1=string1;
}

void Processor::setSpecString2 (int string2) {
	this->string2=string2;
}

void Processor::setSpecPkgType (int pkgType) {
	this->pkgType=pkgType;
}

void Processor::setBoostStates(int numBoostStates)
{
	this->numBoostStates = numBoostStates;
}

DWORD Processor::getBoost(void)
{
	return -1;
}

void Processor::setBoost(bool boost)
{
	printf ("Unsupported processor feature\n");
	return;
}

void Processor::setNumBoostStates(DWORD numBoostStates)
{
	printf ("Unsupported processor feature\n");
	return;
}

void Processor::setTDP(int TDP)
{
	this->TDP = TDP;
}

void Processor::setMaxSlots(int maxslots)
{
	this->maxslots = maxslots;
}

int Processor::getSpecFamilyBase () {
	return this->familyBase;
}

int Processor::getSpecModel () {
	return this->model;
}

int Processor::getSpecStepping () {
	return this->stepping;
}

int Processor::getSpecFamilyExtended () {
	return this->familyExtended;
}

int Processor::getSpecModelExtended () {
	return this->modelExtended;
}

int Processor::getSpecBrandId () {
	return this->brandId;
}

int Processor::getSpecProcessorModel () {
	return this->processorModel;
}

int Processor::getSpecString1 () {
	return this->string1;
}

int Processor::getSpecString2 () {
	return this->string2;
}

int Processor::getSpecPkgType () {
	return this->pkgType;
}

int Processor::getBoostStates ()
{
	return this->numBoostStates;
}

DWORD Processor::getTDP ()
{
	return this->TDP;
}


int Processor::getMaxSlots ()
{
	return this->maxslots;
}

/*** Following methods are likely to be overloaded/overridden inside each module ***/

/* setVID */
void Processor::setVID(PState ps, DWORD vid) {
	printf ("Processor::setVID()\n");
	return;
}

/* setFID */

void Processor::setFID(PState ps, float fid) {
	return;
}

/* setDID */

void Processor::setDID(PState ps, float did) {
	return;
}

/*
 * get* methods allows to gather specific vid/fid/did parameter.
 * Here we have just one complete variant (instead of three different
 * variant as set* methods have) with four parameters because
 * these methods return a single value. To avoid overlapping
 * and useless contradictions, and to make code more compact,
 * I prefer to make definitions of these methods as precise
 * as possible
 */

/* getVID */

DWORD Processor::getVID(PState ps) {
	return -1;
}

/* getFID */

float Processor::getFID(PState ps) {
	return -1;
}

/* getDID */

float Processor::getDID(PState ps) {
	return -1;
}


/* setFrequency */

void Processor::setFrequency(PState ps, DWORD frequency) {
	return;
}

/* setVCore */

void Processor::setVCore(PState ps, float vcore) {
	return;
}

/* getFrequency */

DWORD Processor::getFrequency(PState ps) {
	return -1;
}

/* getVCore */

float Processor::getVCore(PState ps) {
	return -1;
}

/* pStateEnable */
void Processor::pStateEnable(PState ps) {
	return;
}

/* pStateDisable */
void Processor::pStateDisable(PState ps) {
	return;
}

/* pStateEnabled (peeking) */
bool Processor::pStateEnabled(PState ps) {
	return false;
}

//Primitives to set maximum p-state
void Processor::setMaximumPState(PState ps) {
	printf("Unsupported processor feature\n");
	return;
}

PState Processor::getMaximumPState() {
	printf("Unsupported processor feature\n");
	return 0;
}

/* setNBVid */
void Processor::setNBVid(DWORD vid) {
	printf("Unsupported processor feature\n");
	return;
}

void Processor::setNBVid(PState ps, DWORD vid) {
	printf("Unsupported processor feature\n");
	return;
}

/* setNBDid */
void Processor::setNBDid(PState ps, DWORD did) {
	printf("Unsupported processor feature\n");
	return;
}


/* getNBVid */
DWORD Processor::getNBVid() {
	printf("Unsupported processor feature\n");
	return -1;
}

DWORD Processor::getNBVid(PState ps) {
	printf("Unsupported processor feature\n");
	return -1;
}

/* getNBDid */
DWORD Processor::getNBDid(PState ps) {
	printf("Unsupported processor feature\n");
	return -1;
}

/* setNBFid */
void Processor::setNBFid(DWORD fid) {
	printf("Unsupported processor feature\n");
	return;
}

/* getNBFid */
DWORD Processor::getNBFid() {
	printf ("Unsupported processor feature\n");
	return -1;
}

/* getMaxNBFrequency */
DWORD Processor::getMaxNBFrequency() {
	printf("Unsupported processor feature\n");
	return -1;
}

bool Processor::getPVIMode() {
	return false;
}

void Processor::forcePState(PState ps) {
	return;
}
bool Processor::getSMAF7Enabled() {
	return false;
}
DWORD Processor::c1eDID() {
	return -1;
}
DWORD Processor::minVID() {
	return -1;
}
DWORD Processor::maxVID() {
	return -1;
}
DWORD Processor::startupPState() {
	return -1;
}
DWORD Processor::maxCPUFrequency() {
	return 0;
} // 0 means that there

//Temperature registers
DWORD Processor::getTctlRegister(void) {
	return -1;
}

DWORD Processor::getTctlMaxDiff(void) {
	return -1;
}

//Voltage slamming time registers
DWORD Processor::getSlamTime(void) {
	return -1;
}
void Processor::setSlamTime(DWORD slamTime) {
	return;
}

DWORD Processor::getAltVidSlamTime(void) {
	return -1;
}
void Processor::setAltVidSlamTime(DWORD slamTime) {
	return;
}

//Voltage ramping time registers - Taken from Phenom Datasheets, not official on turions
DWORD Processor::getStepUpRampTime(void) {
	return -1;
}
DWORD Processor::getStepDownRampTime(void) {
	return -1;
}

void Processor::setStepUpRampTime(DWORD) {
	return;
}

void Processor::setStepDownRampTime(DWORD) {
	return;
}

//HTC Section
bool Processor::HTCisCapable() {
	return false;
}

bool Processor::HTCisEnabled() {
	return false;
}

bool Processor::HTCisActive() {
	return false;
}

bool Processor::HTChasBeenActive() {
	return false;
}

DWORD Processor::HTCTempLimit() {
	return -1;
}

bool Processor::HTCSlewControl() {
	return false;
}

DWORD Processor::HTCHystTemp() {
	return -1;
}

DWORD Processor::HTCPStateLimit() {
	return -1;
}

bool Processor::HTCLocked() {
	return false;
}

DWORD Processor::getAltVID() {
	return -1;
}

//HTC Section - Change status

void Processor::HTCEnable() {
	return;
}

void Processor::HTCDisable() {
	return;
}
void Processor::HTCsetTempLimit(DWORD tempLimit) {
	return;
}
void Processor::HTCsetHystLimit(DWORD hystLimit) {
	return;
}
void Processor::setAltVid(DWORD altvid) {
	return;
}

//PSI_L bit

bool Processor::getPsiEnabled() {
	return false;
}
DWORD Processor::getPsiThreshold() {
	return -1;
}
void Processor::setPsiEnabled(bool toggle) {
	return;
}
void Processor::setPsiThreshold(DWORD threshold) {
	return;
}

//Hypertransport Section
void Processor::setHTLinkSpeed(DWORD, DWORD) {
	return;
}

//Various settings

bool Processor::getC1EStatus() {
	return false;
}
void Processor::setC1EStatus(bool toggle) {
	return;
}

//performance counters section

void Processor::perfCounterGetInfo() {
	return;
}

void Processor::perfCounterGetValue(unsigned int perfCounter) {
	return;
}

void Processor::perfMonitorCPUUsage() {
	return;
}

void Processor::perfMonitorFPUUsage() {
	return;
}

void Processor::perfMonitorDCMA() {
	return;
}

void Processor::checkMode() {
	return;
}

//Scaler helper methods and structes
void Processor::getCurrentStatus(struct procStatus *, DWORD) {
	return;
}
