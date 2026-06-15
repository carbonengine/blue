// Copyright © 2013 CCP ehf.

#pragma once
#ifndef ScopedBlockTrap_h
#define ScopedBlockTrap_h

#include <CcpMacros.h>

// Forward declare type from the <Scheduler.h> header, include will be in .cpp file
struct PyTaskletObject;

// Description:
//   ScopedBlockTrap provides a way to set and restore blocktrap state around
//   a scope, such as a C++ to python callback
class BLUEIMPORT ScopedBlockTrap
{
#if CCP_STACKLESS
private:
	int m_originalBlocktrapState;
	PyTaskletObject* m_tasklet;
#endif
public:
	// Set the blocktrap if able
	ScopedBlockTrap();

	// Restore the blocktrap to its original state
	~ScopedBlockTrap();
};

#endif // ScopedBlockTrap_h
