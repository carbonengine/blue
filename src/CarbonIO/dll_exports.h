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
void CioSetOnTaskletScheduledCallback( void (*callback)(bool force) );
void CioWakeupTasklets();

bool CioSendPacket( const long long fd, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
unsigned int CioGetMaxPacketsize( const unsigned int len, const unsigned int OOBLen );
unsigned int CioFormatPacket( char* buf, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
bool CioSendFormattedPacket( const long long fd, const char* data, const unsigned int len );

// API servicing calls
typedef bool(*CioDataCallback)(long long descriptor, const char* data, const int len, const char* OOBdata, const int OOBLen );

void CioAddPacketCallbackPostDecompress( CioDataCallback packetCallback );
void CioRemovePacketCallbackPostDecompress( CioDataCallback packetCallback );

void CioSetErrorLogCallback( void (*callback)(const char* msg) );
void CioSetStatusLogCallback( void (*callback)(const char* msg) );

enum WakeupMethod
{
	WAKEUP_DYNAMIC_CONTEXT = 1,
	WAKEUP_PENDING_CALL,
	WAKEUP_LAST
};
PyObject* CioSetWakeupMethod( int method );
PyObject* CioGetWakeupMethod();


#endif
