/*************************************************************************

dll_export.cpp

Author:    Curt Hartung
Created:   Sep 2010
OS:        Win32
Project:   CarbonIO

Description: implementation of the DLL exports

(c) CCP 2010

***************************************************************************/

#include "StdAfx.h"

#include "CarbonIO.h" // to ensure winsock2.h is included before windows.h/Python.h
#include "dll_exports.h"

//------------------------------------------------------------------------------
void CioSetOnTaskletScheduledCallback( void (*callback)(bool force) )
{
	CarbonIO::singleton()->setOnTaskletScheduledCallback( callback );
}

//------------------------------------------------------------------------------
void CioWakeupTasklets()
{
	CarbonIO::singleton()->wakeupTasklets();
}

//------------------------------------------------------------------------------
bool CioSendPacket( const long long fd, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen )
{
	return CarbonIO::singleton()->externalSendPacket( (const SOCKET)fd, data, len, OOBData, OOBLen );
}

//------------------------------------------------------------------------------
unsigned int CioGetMaxPacketsize( const unsigned int len, const unsigned int OOBLen )
{
	return CarbonIO::singleton()->getMaxPacketSize( len, OOBLen );
}

//------------------------------------------------------------------------------
unsigned int CioFormatPacket( char* buf, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen )
{
	unsigned int outlen = 0;
	CarbonIO::singleton()->formatPacket( buf, &outlen, data, len, OOBData, OOBLen, false );
	return outlen;
}

//------------------------------------------------------------------------------
bool CioSendFormattedPacket( const long long fd, const char* data, const unsigned int len )
{
	return CarbonIO::singleton()->externalSendFormattedPacket( (const SOCKET)fd, data, len );
}

//------------------------------------------------------------------------------
void CioAddPacketCallbackPostDecompress( CioDataCallback packetCallback )
{
	CarbonIO::singleton()->addPacketCallbackPostDecompress( packetCallback );
}

//------------------------------------------------------------------------------
void CioRemovePacketCallbackPostDecompress( CioDataCallback packetCallback )
{
	CarbonIO::singleton()->removePacketCallbackPostDecompress( packetCallback );
}

//------------------------------------------------------------------------------
void CioSetErrorLogCallback( void (*callback)(const char* msg) )
{
	CarbonIO::singleton()->setErrorLogCallback( callback );
}

//------------------------------------------------------------------------------
void CioSetStatusLogCallback( void (*callback)(const char* msg) )
{
	CarbonIO::singleton()->setStatusLogCallback( callback );
}

//------------------------------------------------------------------------------
PyObject* CioSetWakeupMethod( int method )
{
	return CarbonIO::singleton()->setWakeupMethod( method );
}

//------------------------------------------------------------------------------
PyObject* CioGetWakeupMethod()
{
	return CarbonIO::singleton()->getWakeupMethod();
}
