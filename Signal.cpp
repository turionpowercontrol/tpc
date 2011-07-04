/*
 * Signal.cpp
 *
 *  Created on: 04/lug/2011
 *      Author: paolo
 */

#include "Signal.h"

bool Signal::signaled=false;

void Signal::signalHandler (int signo) {

	signaled=true;

}

void Signal::activateSignalHandler (int signo) {

	signaled=false;
	signal (signo, signalHandler);

}

bool Signal::getSignalStatus () {

	return signaled;

}
