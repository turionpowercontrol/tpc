
#include <string.h>
#include "config.h"
#include "scaler.h"


//Takes care of a configuration file

CfgManager::CfgManager (class Processor *prc, class Scaler *sclr) {

	cfgFile=NULL;
	processor=prc;
	scaler=sclr;
}

//Open the file. Returns false if file is opened correctly, else returns true
bool CfgManager::openCfgFile (char *txtFile) {

	cfgFile=fopen (txtFile,"r");
	
	printf ("Configuration file: %s\n",txtFile);
	
	if (cfgFile==NULL) {
		cfgFile=NULL;
		return true;
	}
	
	return false;
	
}

//Reads and parses a PState section in the configuration file.
//Only some commands are available for a pstate section, for example
//vid, fid and did commands... topLine is the row containing PState
//and core identifiers.
int CfgManager::consumePStateSection () {

	unsigned int pstateId=-1;
	unsigned int coreId=-1;
	unsigned int nodeId=-1;
	char line[256];
	
	//-1 means "do not touch".
	int vid=-1, fid=-1, did=-1, enable=-1, frequency=-1, nbvid=-1;
	float voltage=-1,nbvoltage=-1;
	
	char *token;
	
	fgets (line,256,cfgFile);
	
	token=strtok (line," ");
	
	while (token!=NULL) {
	
		if (strcmp(token,"pstate")==0) {
			//found pstate identifier. Throws an error if there are multiple pstate identifiers in a single statement
			token=strtok (NULL," ");
			if (pstateId!=-1) return true;
			sscanf (token,"%u",&pstateId);
		} else if (strcmp(token,"core")==0) {
			//found core identifier. Throws and error if there are multiple core identifiers in a single statement
			token=strtok (NULL," ");
			if (coreId!=-1) return true;
			sscanf (token,"%u",&coreId);
		} else if (strcmp(token,"node")==0) {
			//found node identifier. Throws and error if there are multiple nodes identifiers in a single statement
			token=strtok (NULL," ");
			if (nodeId!=-1) return true;
			sscanf (token,"%d",&nodeId);
		} else {
			//Found an unknown identifier, throws error.
			return true;
		}
	
		token=strtok (NULL," ");
	
	}	
	
	//Let's scan for internal identifiers, like vid, fid, did, etc...
	
	while (true) {
	
		if (fscanf (cfgFile, "%s", line)==-1) break;
		
		if (strcmp (line,"#")==0) {
			//This is a comment, then read the whole line and go ahead
			fgets (line,256,cfgFile);
		} else if (strcmp (line,"vid")==0) {
			fscanf (cfgFile, "%d", &vid);
		} else if (strcmp (line,"fid")==0) {
			fscanf (cfgFile, "%d", &fid);
		} else if (strcmp (line,"did")==0) {
			fscanf (cfgFile, "%d", &did);
		} else if (strcmp (line,"enable")==0) {
			enable=1;
		} else if (strcmp (line,"disable")==0) {
			enable=0;
		} else if (strcmp (line,"frequency")==0) {
			fscanf (cfgFile, "%d", &frequency);
		} else if (strcmp (line,"voltage")==0) {
			fscanf (cfgFile, "%f", &voltage);
		} else if (strcmp (line,"nbvoltage")==0) {
			fscanf (cfgFile, "%f", &nbvoltage);
		} else if (strcmp (line,"nbvid")==0) {
			fscanf (cfgFile, "%d", &nbvid);
		} else if (strcmp (line,":")==0) {
			//Found new statement, let's reposition the reading pointer and return
			fseek (cfgFile, -1, SEEK_CUR);
			break;
		} else {
			printf ("Unknown identifier: %s\n", line);
			return true;
		}
		
	}
	
	if (pstateId==-1) return true;
	
	PState pstate (pstateId);
	
	//Select nodes to operate on
	if (nodeId==-1)
		processor->setNode(processor->ALL_NODES);
	else
		processor->setNode(nodeId);

	//Select cores to operate on
	if (coreId==-1)
		processor->setCore(processor->ALL_CORES);
	else
		processor->setCore(coreId);

	//Applies FID and DID commands, but only if frequency command is not present, else applies frequency command
	if (frequency==-1) {
		if (did!=-1) processor->setDID (pstate, did);
		if (fid!=-1) processor->setFID (pstate, fid);
	} else  {
		processor->setFrequency (pstate, frequency);
	}

	//Applies VID commands, but only if voltage command is not present, else applies voltage command
	if (voltage==-1) {
		if (vid!=-1) processor->setVID (pstate, vid);
	} else {
		processor->setVCore (pstate, voltage);
	}

	//Enables or disables a pstate
	if (enable==0) processor->pStateDisable (pstate);
	if (enable==1) processor->pStateEnable (pstate);

	//Applies nbvoltage and nbvid commands
	if (nbvoltage!=-1) processor->setNBVid (pstate, processor->convertVcoretoVID(nbvoltage));
	if (nbvid!=-1) processor->setNBVid (pstate, nbvid);
	
	return false;

}

int CfgManager::consumeGeneralSection () {

	PState ps(0);
	char line [256];
	
	int temp;
	float ftemp;

	//Let's scan for internal identifiers, like psmax, etc...
	
	processor->setCore(processor->ALL_CORES);
	processor->setNode(processor->ALL_NODES);

	while (true) {
	
		if (fscanf (cfgFile, "%s", line)==-1) break;
		
		//This are generic items for comments or ending sections, don't change them
		
		if (strcmp (line,"#")==0) {
			//This is a comment, then read the whole line and go ahead
			fgets (line,256,cfgFile);
		} else if (strcmp (line,":")==0) {
			//Found new statement, let's reposition the reading pointer and return
			fseek (cfgFile, -1, SEEK_CUR);
			break;
			
		// ********** These items controls general power management behaviour

		} else if (strcmp (line,"set node")==0) {
			fscanf (cfgFile, "%u", &temp);
			processor->setNode(temp);
		} else if (strcmp (line,"psmax")==0) {
			fscanf (cfgFile, "%d", &temp);
			ps.setPState (temp);
			processor->setMaximumPState (ps);
		} else if (strcmp (line,"nbvid")==0) {
			fscanf (cfgFile, "%d", &temp);
			processor->setNBVid (temp);
		} else if (strcmp (line,"altvid")==0) {
			fscanf (cfgFile, "%d", &temp);
			processor->setAltVid (temp);
		} else if (strcmp (line,"slamtime")==0) {
			fscanf (cfgFile, "%d", &temp);
			processor->setSlamTime (temp);
		} else if (strcmp (line,"altvidslamtime")==0) {
			fscanf (cfgFile, "%d", &temp);
			processor->setAltVidSlamTime (temp);
		} else if (strcmp (line,"psienable")==0) {
			processor->setPsiEnabled (true);
		} else if (strcmp (line,"psidisable")==0) {
			processor->setPsiEnabled (false);
		} else if (strcmp (line,"psithreshold")==0) {
			fscanf (cfgFile, "%d", &temp);
			processor->setPsiThreshold (temp);
		} else if (strcmp (line,"C1Eenable")==0) {
			fscanf (cfgFile,"%d", &temp);
			processor->setCore(temp);
			processor->setC1EStatus (true);
		} else if (strcmp (line,"C1Edisable")==0) {
			fscanf (cfgFile,"%d", &temp);
			processor->setCore(temp);
			processor->setC1EStatus (false);
		} else if (strcmp (line,"nbvoltage")==0) {
			fscanf (cfgFile,"%f", &ftemp);
			processor->setNBVid (processor->convertVcoretoVID(ftemp));
		}
		
		
		// ************** Bad unknown items
		else {
			printf ("Unknown identifier: %s\n", line);
			return true;
		}	
			
	}
	
	return false;

}

int CfgManager::consumeScalerSection () {

	char line [256];
	char strTemp[256];
	
	int temp;

	//Let's scan for internal identifiers, like uppolicy, downpolicy, samplingrate
	
	while (true) {
	
		if (fscanf (cfgFile, "%s", line)==-1) break;
		
		//This are generic items for comments or ending sections, don't change them
		
		if (strcmp (line,"#")==0) {
			//This is a comment, then read the whole line and go ahead
			fgets (line,256,cfgFile);
		} else if (strcmp (line,":")==0) {
			//Found new statement, let's reposition the reading pointer and return
			fseek (cfgFile, -1, SEEK_CUR);
			break;
			
		// ********** These items controls general power management behaviour
					
		} else if (strcmp (line,"samplingrate")==0) {
			fscanf (cfgFile, "%d", &temp);
			scaler->setSamplingFrequency (temp);
		} else if (strcmp (line,"uppolicy")==0) {
			fscanf (cfgFile, "%s", strTemp);
			if (strcmp(strTemp,"rocket")==0)
				scaler->setUpPolicy (POLICY_ROCKET);
			else if (strcmp(strTemp,"step")==0)
				scaler->setUpPolicy (POLICY_STEP);
			else if (strcmp(strTemp,"dynamic")==0)
				scaler->setUpPolicy (POLICY_DYNAMIC);
			else return true;

		} else if (strcmp (line,"downpolicy")==0) {
			fscanf (cfgFile, "%s", strTemp);
			if (strcmp(strTemp,"rocket")==0)
				scaler->setDownPolicy (POLICY_ROCKET);
			else if (strcmp(strTemp,"step")==0)
				scaler->setDownPolicy (POLICY_STEP);
			else if (strcmp(strTemp,"dynamic")==0)
				scaler->setDownPolicy (POLICY_DYNAMIC);
			else return true;
			
		} else if (strcmp (line,"upperthreshold")==0) {
			fscanf (cfgFile, "%d", &temp);
			if ((temp<0) || (temp>100)) return true;
			scaler->setUpperThreshold (temp);
		} else if (strcmp (line,"lowerthreshold")==0) {
			fscanf (cfgFile, "%d", &temp);
			if ((temp<0) || (temp>100)) return true;
			scaler->setLowerThreshold (temp);
		}
		
		
		// ************** Bad unknown items
		else {
			printf ("Unknown identifier: %s\n", line);
			return true;
		}	
			
	}
	
	return false;

}


//Parse configuration file. Returns 0 (false) if parsed correctly, else returns
//the row of the error in the cfg file.
int CfgManager::parseCfgFile () {

	printf ("Parsing configuration...\n");
	
	char line[256];

	if (cfgFile==NULL) return 0;
	
	while (!feof (cfgFile)) {
	
		//reads a string, returns -1 if there are no more string (so file is finished)
		if (fscanf (cfgFile,"%s",line)==-1) break;
		
		//Ignores all the comments beginning with # character
		if (strcmp (line,"#")==0) {
			fgets (line,256,cfgFile);
		} else if (strcmp (line,":")==0) {
		
			//The colon (:) determines a statement, next there's an uppercase word that
			//indicates the statement referring to.
			
			//Actually there are two statements, PSTATESET to allow configuring a pstate
			//configuration and GENERAL to allow configuring general settings
		
			fscanf (cfgFile,"%s",line);
			
			if (strcmp (line,"PSTATESET")==0) {
				if (consumePStateSection ()) return ftell (cfgFile);
			} else if (strcmp (line,"GENERAL")==0) {
				if (consumeGeneralSection ()) return ftell (cfgFile);
			} else if (strcmp (line,"SCALER")==0) {
				if (consumeScalerSection ()) return ftell (cfgFile);
			} else {
				return ftell (cfgFile);
			}
			
		} else {
			//Else there is other garbage we don't care about
			fgets (line,256,cfgFile);
			printf ("Invalid data: %s\n", line);
		}
		
	}

	printf ("Configuration file has been parsed!\n");

	return 0;
	
}

bool CfgManager::closeCfgFile () {

	if (cfgFile==NULL) return false;
	
	fclose (cfgFile);
	
	return false;
}

CfgManager::~CfgManager () {

	closeCfgFile ();

}
