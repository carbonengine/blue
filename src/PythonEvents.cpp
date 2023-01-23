#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "PythonEvents.h"

static CcpLogChannel_t s_chPy = CCP_LOG_DEFINE_CHANNEL( "Python Logs" );
static CcpLogChannel_t s_chStdErr = CCP_LOG_DEFINE_CHANNEL( "stderr" );

static const char* NO_LOGGING_PREFIX = "#nolog: ";
static size_t NO_LOGGING_PREFIX_LEN = strlen( NO_LOGGING_PREFIX );

static CCP::LogType FLAGS[] = {
	CCP::LOGTYPE_INFO,
	CCP::LOGTYPE_ERR,
	CCP::LOGTYPE_INFO,
	CCP::LOGTYPE_WARN,
	CCP::LOGTYPE_ERR,
	CCP::LOGTYPE_ERR
};

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

	bool nolog = strncmp( text, NO_LOGGING_PREFIX, NO_LOGGING_PREFIX_LEN ) == 0;

	if (mPort == PYSTDOUT || mPort == PYSTDERR)
	{
		if( sPyEventHandler )
		{
			if( nolog )
			{
				text += NO_LOGGING_PREFIX_LEN;
			}
			sPyEventHandler->OnWrite( mPort, text );
		}
	}

	if (mPort != PYSTDOUT)
	{
		CCP::LogType logflag = FLAGS[mPort];

		if( !nolog && (strcmp( text, "\n" ) != 0 ) )
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

			CCP::LogFuncChannel( *ch, logflag, 0, "%s", text );
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

PyObject* PythonEvents::Pyisatty(PyObject* args)
{
    if (mPort == PYSTDOUT && isatty(fileno(stdout)))
	{
		Py_RETURN_TRUE;
	}
    else if (mPort == PYSTDERR && isatty(fileno(stderr))) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

#endif
