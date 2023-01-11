/*
	*************************************************************************************

	Logger.h

	Author:		Matthias Gudmundsson
	Created:	June 1998
	Revised:	April 1999
	OS:			Win32
	Project:	Utilities

	Description:

		Logging utility - client side.

	(c) OZ.COM 1998, 1999




		Initialization
		__________________________________

		In WinMain or DllMain put this:

		LOGINITIALIZE(hInstance, "OV3");

		This will identify your .dll or .exe and start the log library
		associated with the log device named "OV3".  You pulsu-people
		will put "iPulse" instead of "OV3".

		It's also nice to put LOGTERMINATE() in WinMain or DllMain to
		shut down and clean up resources and stuff.




*/



#ifndef __LOGGER_H__
#define __LOGGER_H__

typedef enum TLOGFLAG
{
    
	LGINFO =		0x01,
	LGWARN =		0x02,
	LGERR =			0x04,
	LGFATAL =		0x08,
    
	LGOVERLAP =		0x10,
	LGPERF =		0x20,
	LGCOUNT =		0x40,
    
} TLOGFLAG;

CCP::LogType TLOGFLAGToLogType( TLOGFLAG flag );

TLOGFLAG LogTypeToTLOGFLAG( CCP::LogType logType );

#endif // __LOGGER_H__

