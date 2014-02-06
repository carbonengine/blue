////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueResFile2.h"
#include "include/IBlueOS.h"

#if USE_RESFILE_2
BLUE_DEFINE( ResFile );
#else
BLUE_DEFINE( BlueResFile2 );
#endif

#if USE_RESFILE_2

#if BLUE_WITH_PYTHON
static PyObject* PyOpen( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	PyObject* filename;
	int readonly = 1;
	if( !PyArg_ParseTuple( args, "O!|i:Open", &PyBaseString_Type, &filename, &readonly ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	bool ok = pThis->OpenW( (const wchar_t*)PyUnicode_AsUnicode( filename ), readonly != 0);

	return PyBool_FromLong( ok );
}

static PyObject* PyOpenAlways( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	PyObject* filename;
	int readonly = 1;
	if( !PyArg_ParseTuple( args, "O!|i:Open", &PyBaseString_Type, &filename, &readonly ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	bool ok = pThis->OpenW( (const wchar_t*)PyUnicode_AsUnicode( filename ), readonly != 0);

	if( !ok )
	{
		PyErr_SetString( PyExc_BlueError, "Couldn't open file" );
		return nullptr;
	}
	
	return BlueWrapObjectForPython( pThis );
}

static PyObject* PyCreate( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	PyObject *filename;

	if( !PyArg_ParseTuple( args, "O!|:Create", &PyBaseString_Type, &filename ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	bool ok = pThis->CreateW( (const wchar_t*)PyUnicode_AsUnicode( filename ) );

	return PyInt_FromLong( ok );
}

static PyObject* PyClose( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	if( !PyArg_ParseTuple( args, ":Close" ) )
	{
		return NULL;
	}

	bool ok = pThis->Close();

	return PyInt_FromLong( ok );
}

static PyObject* PyRead( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	int size = -1;
	if( !PyArg_ParseTuple( args, "|i:read", &size ) )
	{
		return NULL;
	}

	if( size < 0 )
	{
		// Read the rest of the file
		size = (int)(pThis->GetSize() - pThis->GetPosition());
	}

	if( size < 0 )
	{
		// Somehow we've advanced beyond the end of the file
		return NULL;
	}

	PyObject* str = PyString_FromStringAndSize( 0, size );
	if( !str )
	{
		return NULL;
	}

	if( size == 0 )
	{
		// 0 bytes requested - presumably we're at the end of the file.
		// Return the empty string.
		return str;
	}

	ssize_t bytesRead = pThis->Read( PyString_AS_STRING( str ), size );

	if( bytesRead < 0 )
	{
		// Read error
		Py_DECREF( str );
		return NULL;
	}

	if( bytesRead != size )
	{
		// We read something, but not the full amount requested. Resize
		// the string to the amount read.
		if( _PyString_Resize( &str, bytesRead ) != 0 )
		{
			// Resize failed
			return NULL;
		}
	}

	return str;
}

static PyObject* PySeek( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	int offset = 0;
	int whence = 0;

	if( !PyArg_ParseTuple( args, "i|i:seek", &offset, &whence ) )
	{
		return NULL;
	}

	pThis->Seek( offset, (BLUESEEK)whence );

	Py_RETURN_NONE;
}

#endif
#endif

const Be::ClassInfo* RESFILE_2_CLASSNAME::ExposeToBlue()
{
#if USE_RESFILE_2
	EXPOSURE_BEGIN( ResFile, "" );
#else
	EXPOSURE_BEGIN( BlueResFile2, "" );
#endif
		MAP_INTERFACE( IResFile )
		MAP_INTERFACE( IBlueStream )

#if USE_RESFILE_2
#if BLUE_WITH_PYTHON
		MAP_METHOD
		(
			"Open",
			PyOpen,
			"Opens the file object to the given path."
			"\n"
			"\nArguments:"
			"\n filename"
			"\nReturns:"
			"\n  True on success, False on failure"
		)

		MAP_METHOD
		(
			"OpenAlways",
			PyOpenAlways,
			""
		)

		MAP_METHOD
		(
			"open",
			PyOpenAlways,
			""
		)

		MAP_METHOD
		(
			"Create",
			PyCreate,
			"Creates a file on disk with the given filename."
			"\n"
			"\nArguments:"
			"\n filename"
			"\nReturns:"
			"\n  True on success, False on failure"
		)

		MAP_METHOD
		(
			"close",
			PyClose,
			"Closes the file."
		)

		MAP_METHOD
		(
			"Close",
			PyClose,
			"Closes the file."
		)

		MAP_METHOD
		(
			"read",
			PyRead,
			"Reads the contents of the file, or the next given number of bytes."
			"\n"
			"\nArguments:"
			"\n size (optional) - how many bytes to read"
			"\nReturns:"
			"\n A string with the bytes read"
		)

		MAP_METHOD
		(
			"seek",
			PySeek,
			"Set the file current position. See Python file object docs for details."
			"\n"
			"\nArguments:"
			"\n offset - where to seek to"
			"\n whence (optional) - where to seek from. Defaults to 0 for absolute position."
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExists",
			FileExists,
			"Deprecated: Use blue.paths.FileExists"
		)

		MAP_PROPERTY_READONLY
		(
			"size",
			GetSize,
			"The size of the file"
		)
#endif
#endif
	EXPOSURE_END()
}
