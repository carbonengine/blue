#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "PythonEvents.h"

static CcpLogChannel_t s_chPy = CCP_LOG_DEFINE_CHANNEL( "Python Logs" );
static CcpLogChannel_t s_chStdErr = CCP_LOG_DEFINE_CHANNEL( "stderr" );

//////////////////////////////////////////////////////////////////////
//
// PythonEvents class
//
//////////////////////////////////////////////////////////////////////

extern IPythonEventsPtr sPyEventHandler;


//--------------------------------------------------------------------
// PythonEvents::Pywrite(
//--------------------------------------------------------------------
PyObject* PythonEvents::Pywrite(PyObject* args)
{
	const char *text;

	if (!PyArg_ParseTuple(args, "s", &text))
		return NULL;

	if (mPort == PYSTDOUT || mPort == PYSTDERR)
	{
		if (sPyEventHandler)
			sPyEventHandler->OnWrite(mPort, text);
	}

	if (mPort != PYSTDOUT)
	{
		CCP::LogType FLAGS[] = {
			CCP::LOGTYPE_INFO, 
			CCP::LOGTYPE_ERR, 
			CCP::LOGTYPE_INFO, 
			CCP::LOGTYPE_WARN, 
			CCP::LOGTYPE_ERR, 
			CCP::LOGTYPE_ERR
		};
		CCP::LogType logflag = FLAGS[mPort];

		// Need to feed the logger with one line at a time.
		while(*text)
		{
			const char *lf = strchr(text, '\n');
			int len;
			if( lf )
			{
				len = (int)(lf-text);
			}
			else
			{
				len = (int)strlen(text);
			}

			if( len )
			{
				CcpLogChannel_t* ch;
				if( mPort == PYSTDERR )
				{
					ch = &s_chStdErr;
				}
				else
				{
					ch = &s_chPy;
				}

				CCP::LogFuncChannel( *ch, logflag, 0, "%.*s", len, text );
			}
			if( !lf )
			{
				break;
			}
			text += len+1;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

//--------------------------------------------------------------------
// PythonEvents::Pyflush(
//--------------------------------------------------------------------
PyObject* PythonEvents::Pyflush(PyObject* args)
{
	if (mPort == PYSTDOUT)
	{
		fflush(stdout);
	}
	else if(mPort == PYSTDERR)
	{
		fflush(stderr);
	}
	Py_RETURN_NONE;
}

#endif
