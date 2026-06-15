// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueResFile2.h"
#include "IBlueOS.h"

BLUE_DEFINE( ResFile );

#if BLUE_WITH_PYTHON
static PyObject* PyOpen( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	PyObject* filename;
	int readonly = 1;
	if( !PyArg_ParseTuple( args, "O!|i:Open", &PyUnicode_Type, &filename, &readonly ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	wchar_t* fileNameWCString = PyUnicode_AsWideCharString( filename, NULL );

	bool ok = pThis->OpenW( fileNameWCString, readonly != 0 );

	PyMem_Free( fileNameWCString );

	return PyBool_FromLong( ok );
}

static PyObject* PyOpenAlways( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	PyObject* filename;
	int readonly = 1;
	if( !PyArg_ParseTuple( args, "O!|i:Open", &PyUnicode_Type, &filename, &readonly ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	wchar_t* fileNameWCString = PyUnicode_AsWideCharString( filename, NULL );

	bool ok = pThis->OpenW( fileNameWCString, readonly != 0 );

	PyMem_Free( fileNameWCString );

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

	if( !PyArg_ParseTuple( args, "O!|:Create", &PyUnicode_Type, &filename ) )
	{
		return NULL;
	}

	filename = PyUnicode_FromObject( filename );
	if( !filename )
	{
		return NULL;
	}

	wchar_t* fileNameWCString = PyUnicode_AsWideCharString( filename, NULL );

	bool ok = pThis->CreateW( fileNameWCString );

	PyMem_Free( fileNameWCString );

	return PyLong_FromLong( ok );
}

static PyObject* PyClose( PyObject* self, PyObject* args )
{
	ResFile* pThis = BluePythonCast<ResFile*>( self );

	if( !PyArg_ParseTuple( args, ":Close" ) )
	{
		return NULL;
	}

	bool ok = pThis->Close();

	return PyLong_FromLong( ok );
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

	PyObject* str = PyBytes_FromStringAndSize( 0, size );
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

	ssize_t bytesRead = pThis->Read( PyBytes_AS_STRING( str ), size );

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
		if( _PyBytes_Resize( &str, bytesRead ) != 0 )
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

	pThis->Seek( offset, (ICcpStream::SeekOrigin)whence );

	Py_RETURN_NONE;
}

#endif

const Be::ClassInfo* ResFile::ExposeToBlue()
{
	EXPOSURE_BEGIN( ResFile, "" );
		MAP_INTERFACE( IResFile )
		MAP_INTERFACE( IBlueStream )

#if BLUE_WITH_PYTHON
		MAP_METHOD
		(
			"Open",
			PyOpen,
			"Opens the file object to the given path.\n"
			":param filename: path to file\n"
			":type filename: basestring\n"
			":param readonly: open for reading or read/write\n"
			":type readonly: Optional[int]\n"
			":rtype: bool"
		)

		MAP_METHOD
		(
			"OpenAlways",
			PyOpenAlways,
			"Opens the file object to the given path.\n"
			":param filename: path to file\n"
			":type filename: basestring\n"
			":param readonly: open for reading or read/write\n"
			":type readonly: Optional[int]\n"
			":rtype: ResFile"
		)

		MAP_METHOD
		(
			"open",
			PyOpenAlways,
			"Opens the file object to the given path.\n"
			":param filename: path to file\n"
			":type filename: basestring\n"
			":param readonly: open for reading or read/write\n"
			":type readonly: Optional[int]\n"
			":rtype: ResFile"
		)

		MAP_METHOD
		(
			"Create",
			PyCreate,
			"Creates a file on disk with the given filename.\n"
			":param filename: path to file\n"
			":type filename: basestring\n"
			":returns: 1 on success, 0 on failure\n"
			":rtype: int"
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
			"Closes the file.\n"
			":returns: 1 on success, 0 on failure\n"
			":rtype: int"
		)

		MAP_METHOD
		(
			"read",
			PyRead,
			"Reads the contents of the file, or the next given number of bytes.\n"
			":param size: (optional) how many bytes to read\n"
			":type size: Optional[int]\n"
			":returns: A string with the bytes read\n"
			":rtype: str"
		)

		MAP_METHOD
		(
			"seek",
			PySeek,
			"Set the file current position. See Python file object docs for details.\n"
			":param offset: where to seek to\n"
			":type offset: int\n"
			":param whence: (optional) where to seek from. Defaults to 0 for absolute position.\n"
			":type whence: int\n"
			":rtype: None"
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExists",
			FileExists,
			"Deprecated: Use blue.paths.FileExists\n"
			":param fileName:"
		)

		MAP_PROPERTY_READONLY
		(
			"size",
			GetSize,
			"The size of the file"
		)
#endif
	EXPOSURE_END()
}
