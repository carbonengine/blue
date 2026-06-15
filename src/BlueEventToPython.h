// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BlueEventToPython_h
#define BlueEventToPython_h

#include "IBlueEventListener.h"

BLUE_DECLARE( BlueEventToPython );

class BlueEventToPython:
     public IBlueEventListener
{
public:
    EXPOSE_TO_BLUE();

    BlueEventToPython( IRoot* lockobj = NULL );
	~BlueEventToPython();


	//////////////////////////////////////////////////////////////////////////
	// IBlueEventListener
	void HandleEvent( const wchar_t* evtName ) override;

private:
	PyObject* m_handler;
};

TYPEDEF_BLUECLASS( BlueEventToPython );
#endif //BlueEventToPython_h
