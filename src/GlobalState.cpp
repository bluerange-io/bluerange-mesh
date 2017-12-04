/*
 * GlobalState.cpp
 *
 *  Created on: 10.03.2017
 *      Author: marius
 */

#include "GlobalState.h"

/**
 * The GlobalState was introduced to create multiple instances of FruityMesh
 * in a single process. This lets us do some simulation.
 */
GlobalState* GlobalState::instance;

#ifndef SIM_ENABLED
__attribute__ ((section (".noinit"))) RamRetainStruct ramRetainStruct;
__attribute__ ((section (".noinit"))) u32 rebootMagicNumber;
#else
RamRetainStruct ramRetainStruct;
u32 rebootMagicNumber;
#endif

GlobalState::GlobalState()
{
	//Some initialization
	currentEvent = (ble_evt_t *) currentEventBuffer;
	sizeOfCurrentEvent = 0;
	ramRetainStructPtr = &ramRetainStruct;
	rebootMagicNumberPtr = &rebootMagicNumber;
}


GlobalState* GlobalState::getInstance(){
	if(!instance){
		instance = new GlobalState();
	}


	return instance;
}

