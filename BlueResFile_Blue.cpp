////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		May 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#if !USE_RESFILE_2

#include "BlueResFile.h"

BLUE_DEFINE( ResFile );

//--------------------------------------------------------------------
// Open
//--------------------------------------------------------------------
#if BLUE_WITH_PYTHON
PyObject* ResFile::PyOpen(PyObject* args)
{
	PyObject *filename;
	int readonly = 1;
	const char *mode = 0;

	if (!PyArg_ParseTuple(args, "O!|is:Open", &PyBaseString_Type, &filename, &readonly, &mode))
		return NULL;
	filename = PyUnicode_FromObject(filename);
	if (!filename)
		return NULL;

	//used in the python read() function
	mAscii = false;
	if (mode) {
		if (strchr(mode, 'b'))
			mAscii = false;
		else
			mAscii = true;
	}

	bool error;
	bool ok = OpenWImpl(error, (const wchar_t*)PyUnicode_AsUnicode(filename), readonly ? true : false, false, true);
	Py_DECREF(filename);
	if (error)
		return 0;
	return PyInt_FromLong(ok);
}


PyObject *ResFile::Pyopen(PyObject *args)
{
	PyObject *filename;
	int readonly = 1;
	char *mode = 0;

	if (!PyArg_ParseTuple(args, "O!|is:OpenOnly", &PyBaseString_Type, &filename, &readonly, &mode))
		return NULL;
	filename = PyUnicode_FromObject(filename);
	if (!filename)
		return NULL;

	//used in the python read() function
	mAscii = false;
	if (mode) {
		if (strchr(mode, 'b'))
			mAscii = false;
		else
			mAscii = true;
	}

	bool ok = OpenW((const wchar_t*)PyUnicode_AsUnicode(filename), readonly ? true : false);
	Py_DECREF(filename);
	if (!ok)
		return NULL;

	return BlueWrapObjectForPython(this->GetRawRoot());
}


//--------------------------------------------------------------------
// Create
//--------------------------------------------------------------------
PyObject* ResFile::PyCreate(PyObject* args)
{
	PyObject* filename;
	if (!PyArg_ParseTuple(args, "O!", &PyBaseString_Type, &filename))
		return NULL;
	filename = PyUnicode_FromObject(filename);
	if (!filename)
		return NULL;

	bool ok = CreateW((const wchar_t*)PyUnicode_AsUnicode(filename));
	Py_DECREF(filename);
	if (!ok)
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
// Close
//--------------------------------------------------------------------
PyObject* ResFile::PyClose(PyObject* args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	if (!Close())
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *ResFile::Pyclose(PyObject *args){
	return PyClose(args);
}


#ifdef _WIN32

//--------------------------------------------------------------------
// GetFileInfo
//--------------------------------------------------------------------
PyObject* ResFile::PyGetFileInfo(PyObject* args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	BY_HANDLE_FILE_INFORMATION info;

	if (!GetFileInformationByHandle(mFile, &info))
	{
		BeOS->SetError(BE32, Clsid(), "Cannot get any info on file");
		return nullptr;
	}

	PyObject* t1 = PyLong_FromLongLong(*(__int64*)&info.ftCreationTime);
	PyObject* t2 = PyLong_FromLongLong(*(__int64*)&info.ftLastAccessTime);
	PyObject* t3 = PyLong_FromLongLong(*(__int64*)&info.ftLastWriteTime);


	PyObject* ret = Py_BuildValue(
		"{s:i,s:O,s:O,s:O,s:i,s:i,s:i,s:i,s:i}",
		"dwFileAttributes", info.dwFileAttributes,
		"ftCreationTime", t1,
		"ftLastAccessTime", t2,
		"ftLastWriteTime", t3,
		"dwVolumeSerialNumber", info.dwVolumeSerialNumber,
		"nFileSizeHigh", info.nFileSizeHigh, 
		"nFileSizeLow", info.nFileSizeLow,
		"nNumberOfLinks", info.nNumberOfLinks,
		"nFileIndexHigh", info.nFileIndexHigh,
		"nFileIndexLow", info.nFileIndexLow
		);

	Py_DECREF(t1);
	Py_DECREF(t2);
	Py_DECREF(t3);

	return ret;
}

#endif


PyObject* ResFile::PyFileExists(PyObject* args)
{
	PyObject* filename;
	if (!PyArg_ParseTuple(args, "O!", &PyBaseString_Type, &filename))
		return NULL;
	filename = PyUnicode_FromObject(filename);
	if (!filename)
		return NULL;

	PyObject *res = FileExistsW((const wchar_t*)PyUnicode_AsUnicode(filename))?Py_True:Py_False;
	Py_DECREF(filename);
	Py_INCREF(res);
	return res;
}


PyObject *ResFile::Pyread(PyObject *args)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	int size=-1;
	if (!PyArg_ParseTuple(args, "|i:read", &size))
		return 0;
	if (size < 0)
		size = (int)(GetSize() - GetPosition());

	PyObject* str = PyString_FromStringAndSize(0, size);
	if (!str)
		return NULL;
	if (!size)
		return str;

	ssize_t read = Read(PyString_AS_STRING(str), size);
	if (read < 0) {
		Py_DECREF(str);
		return NULL;
	}

	if (mAscii) {
		//if we are ascii mode, replace \r\n with \n
		char *dst = PyString_AS_STRING(str);
		const char *src = dst;
		for(ssize_t i = 0; i<read; i++) {
			if (src[i] == '\r' && i+1<read && src[i+1] == '\n')
				i++;
			*(dst++) = src[i];
		}
		read = dst-src; //new size
	}
	//resize our new string
	if (read != size && _PyString_Resize(&str, read))
		return 0;

	return str;
}

// This provides a more standard file-like interface than the Seek() function exposed on
// the BlueStream thunker. it is exposed as 'seek'
PyObject *ResFile::PySeekStandard(PyObject *args)
{
	int offset = 0;
	int whence = 0;

	if( !PyArg_ParseTuple( args, "i|i:seek", &offset, &whence ) )
	{
		return NULL;
	}

	Seek( offset, (BLUESEEK)whence );

	Py_RETURN_NONE;
}

//context management, as for file objects
//__enter__ just returns the file itself
PyObject *ResFile::Py__enter__(PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":__enter__"))
		return 0;
	return BlueWrapObjectForPython(this->GetRawRoot());
}

//__close__ closes the file
PyObject *ResFile::Py__exit__(PyObject *args)
{
	PyObject *t, *v, *tb;
	if (!PyArg_ParseTuple(args, "OOO:__exit__", &t, &v, &tb))
		return 0;
	//Close file
	if (!Close())
		return 0;
	Py_RETURN_NONE;
}
#endif

const Be::ClassInfo* ResFile::ExposeToBlue()
{
	EXPOSURE_BEGIN( ResFile, "" )

		MAP_INTERFACE(IResFile)
		MAP_INTERFACE(IBlueStream)
		MAP_INTERFACE(ICacheable)

		MAP_ATTRIBUTE( "size", mSize, "size", Be::READ )
		MAP_ATTRIBUTE( "pos", mPosition, "position", Be::READ )
		MAP_ATTRIBUTE( "filename", mFilename, "Filename", Be::READWRITE | Be::PERSIST )
		MAP_ATTRIBUTE( "softspace", mSoftspace, "", Be::READWRITE )

#if BLUE_WITH_PYTHON
		MAP_METHOD_AS_METHOD
		(
			"open",
			Pyopen, 
			"open" 
		)
			
		MAP_METHOD_AS_METHOD
		(
			"OpenAlways",
			Pyopen, 
			"OpenAlways" 
		)

		MAP_METHOD_AS_METHOD
		(
			"Open",
			PyOpen, 
			"Open" 
		)
			
		MAP_METHOD_AS_METHOD
		(
			"Create",
			PyCreate, 
			"Create" 
		)
			
		MAP_METHOD_AS_METHOD
		(
			"Close",
			PyClose, 
			"Close" 
		)
			
#ifdef _WIN32

		MAP_METHOD_AS_METHOD
		(
			"GetFileInfo",
			PyGetFileInfo, 
			"GetFileInfo" 
		)
#endif
			
		MAP_METHOD_AS_METHOD
		(
			"FileExists",
			PyFileExists, 
			"Exists"
		)
			
		MAP_METHOD_AS_METHOD
		(
			"read",
			Pyread, 
			"python file object api"
		)
			
		MAP_METHOD_AS_METHOD
		(
			"close",
			Pyclose, 
			"python file object api"
		)
			
		MAP_METHOD_AS_METHOD
		(
			"__enter__",
			Py__enter__, 
			""
		)
			
		MAP_METHOD_AS_METHOD
		(
			"__exit__",
			Py__exit__, 
			""
		)

		MAP_METHOD_AS_METHOD
		(
			"seek",
			PySeekStandard,
			"Set the file current position. See Python file object docs for details."
			"\n"
			"\nArguments:"
			"\n offset - where to seek to"
			"\n whence (optional) - where to seek from. Defaults to 0 for absolute position."
		)
#endif

	EXPOSURE_END()
}

#endif
