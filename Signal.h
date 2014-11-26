/*
 * Signal.h
 *
 *  Created on: 04/lug/2011
 *      Author: paolo
 */

#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <signal.h>

class Signal {

public:

	static bool signaled;
	static void signalHandler (int signo);
	static void activateSignalHandler (int signo);
	static bool getSignalStatus ();
	static void activateUserSignalsHandler ();

};

#endif /* SIGNAL_H_ */
