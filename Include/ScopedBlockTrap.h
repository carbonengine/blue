////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		April 2013
// Copyright:	CCP 2013
//

#pragma once
#ifndef ScopedBlockTrap_h
#define ScopedBlockTrap_h

#include "CcpCore/include/CcpMacros.h"

// Forward declaration
struct _tasklet;
typedef _tasklet PyTaskletObject;

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