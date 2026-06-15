// Copyright © 2011 CCP ehf.

#pragma once
#ifndef SIMPLELOG_HPP
#define SIMPLELOG_HPP

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <CcpTime.h>

namespace Ccp
{

//------------------------------------------------------------------------------
// ultra-simple lightweight logging facility to get directly at low-level spammy
// log messages.
class SimpleLog
{
public:

	enum { c_logMaxChars = 2000000 };

	void init( const char *path ) { strcpy(m_filename, path); }
	void out( const char* format, ... )
	{
		struct tm tmbuf;
		time_t tod = time(0);
		memcpy( &tmbuf, localtime(&tod), sizeof(struct tm) );

		sprintf( m_temp, "%02d %02d:%02d:%02d.%03d> ",
				 tmbuf.tm_mday,
				 tmbuf.tm_hour,
				 tmbuf.tm_min,
				 tmbuf.tm_sec,
				 (unsigned int)CcpGetTickCount() % 1000 );

		va_list arg;
		va_start( arg, format );
		int len = vsprintf( m_temp + 17, format, arg ) + 17;
		va_end ( arg );

		if ( m_temp[len - 1] != '\n' )
		{
			m_temp[len++] = '\n';
			m_temp[len] = 0;
		}

		printf( "%s", m_temp );

		FILE *fil = fopen( m_filename, "a" );
		if ( fil )
		{
			fwrite( m_temp, len, 1, fil );
			fclose( fil );
		}
	}

private:
	char m_filename[256];
	char m_temp[c_logMaxChars + 18];
};

}
#endif