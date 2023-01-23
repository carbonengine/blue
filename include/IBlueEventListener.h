#pragma once
#ifndef IBLUEEVENTLISTENER_H
#define IBLUEEVENTLISTENER_H

BLUE_INTERFACE( IBlueEventListener ) : public IRoot
{
	BLUEIMPORT virtual void HandleEvent( const wchar_t* evtName ) = 0;
};

#endif