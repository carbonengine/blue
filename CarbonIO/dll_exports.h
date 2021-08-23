#pragma once
#ifndef DLL_EXPORTS_H
#define DLL_EXPORTS_H
/*************************************************************************

dll_exports.cpp

Author:    Curt Hartung
Created:   Sep 2010
OS:        Win32
Project:   CarbonIO

Description: Functions exported in the DLL

(c) CCP 2010

***************************************************************************/

#include <Python.h>

// if installed, SetEvent() will be called on this handle every
// time a tasklet is scheduled
PyAPI_FUNC(void) CioSetOnTaskletScheduledCallback( void (*callback)(bool force) );
PyAPI_FUNC(void) CioWakeupTasklets();

PyAPI_FUNC(bool) CioSendPacket( const long long fd, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
PyAPI_FUNC(unsigned int) CioGetMaxPacketsize( const unsigned int len, const unsigned int OOBLen );
PyAPI_FUNC(unsigned int) CioFormatPacket( char* buf, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
PyAPI_FUNC(bool) CioSendFormattedPacket( const long long fd, const char* data, const unsigned int len );

// API servicing calls
typedef bool(*CioDataCallback)(long long descriptor, const char* data, const int len, const char* OOBdata, const int OOBLen );

PyAPI_FUNC(void) CioAddPacketCallbackPostDecompress( CioDataCallback packetCallback );
PyAPI_FUNC(void) CioRemovePacketCallbackPostDecompress( CioDataCallback packetCallback );

PyAPI_FUNC(void) CioSetErrorLogCallback( void (*callback)(const char* msg) );
PyAPI_FUNC(void) CioSetStatusLogCallback( void (*callback)(const char* msg) );

enum WakeupMethod
{
	WAKEUP_DYNAMIC_CONTEXT = 1,
	WAKEUP_PENDING_CALL,
	WAKEUP_LAST
};
PyAPI_FUNC(PyObject*) CioSetWakeupMethod( int method );
PyAPI_FUNC(PyObject*) CioGetWakeupMethod();


#endif
