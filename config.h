#include <stdlib.h>
#include <stdio.h>
#include "Processor.h"

class CfgManager {
private:
	FILE *cfgFile;
	class Processor *processor;
	class Scaler *scaler;
	int consumePStateSection ();
	int consumeGeneralSection ();
	int consumeScalerSection ();
public:
	CfgManager (class Processor *, class Scaler *);
	~CfgManager ();
	bool openCfgFile (char*);
	int parseCfgFile ();
	bool closeCfgFile ();
};
