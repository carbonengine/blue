// Copyright (c) OZ.COM 1998, 1999

#include "StdAfx.h"

#include "Logger.h"


CCP::LogType TLOGFLAGToLogType( TLOGFLAG flag )
{
	int result;
	switch( flag & 0xff )
	{
        case LGINFO:
            result = CCP::LOGTYPE_INFO;
            break;
            
        case LGPERF:
            result = CCP::LOGTYPE_NOTICE;
            break;
            
        case LGWARN:
            result = CCP::LOGTYPE_WARN;
            break;
            
        case LGERR:
        case LGFATAL:
            result = CCP::LOGTYPE_ERR;
            break;
            
        default:
            result = CCP::LOGTYPE_INFO;
            break;
	}
    
	return (CCP::LogType)result;
}

TLOGFLAG LogTypeToTLOGFLAG( CCP::LogType logType )
{
	static TLOGFLAG s_logFlags[] = {LGINFO, LGPERF, LGWARN, LGERR};
	return s_logFlags[logType];
}


