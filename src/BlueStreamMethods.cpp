// Copyright © 2013 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "IBluePersist.h"
#include <BlueExposureMacrosDeprecated.h>
#include "IBlueOS.h"

class IBlueStream_Thunk : public IBlueStream
{
public:

	typedef IBlueStream_Thunk _Class;
	typedef IBlueStream _Interface;

	static const Be::IID& IID()
	{
		return GetIBlueStreamIID();
	}

	const Be::Clsid* Clsid()
	{
		return ClassType()->mClassId;
	}

	static const PyMethodDef* Defs()
	{
		THUNKER_BEGIN()
			MAPPYTHON( Read,	"Read" )
			MAPPYTHON( read,	"read" )
			MAPPYTHON( Write,	"Write" )
			MAPPYTHON( write,	"write" )
			MAPPYTHON( Seek,	"Seek" )
			MAPPYTHON( seek,	"seek" )
		THUNKER_END()
	}

	DECLARE_PYMETHODTHUNK( Read );
	DECLARE_PYMETHODTHUNK( read );
	DECLARE_PYMETHODTHUNK( Write );
	DECLARE_PYMETHODTHUNK( write );
	DECLARE_PYMETHODTHUNK( Seek );
	DECLARE_PYMETHODTHUNK( seek );
};

BLUE_REGISTER_THUNKER(IBlueStream_Thunk::Defs(), IBlueStream_Thunk::IID());

//--------------------------------------------------------------------
// IBlueStream::Read
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::Pyread(PyObject* args)
{
	return PyRead( args );
}

PyObject* IBlueStream_Thunk::PyRead(PyObject* args)
{
	Py_ssize_t size = -1;

	if (!PyArg_ParseTuple(args, "|n", &size))
		return NULL;

	//if (!IsOpen())
	//return NULL;

	if (size < 0)
		size = GetSize() - GetPosition();

	PyObject* str = PyBytes_FromStringAndSize(0, size);
	if (!str)
		return NULL;
	if (size) {
		ssize_t read = Read(PyBytes_AS_STRING(str), size);
		if (read < 0) {
			Py_DECREF(str);
			return NULL;
		}
		if (read != size && _PyBytes_Resize(&str, read))
			return 0;
	}
	return str;
}


//--------------------------------------------------------------------
// IBlueStream::Write
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::Pywrite(PyObject* args)
{
	return PyWrite( args );
}

PyObject* IBlueStream_Thunk::PyWrite(PyObject* args)
{
	PyObject* pyobj;
	Py_buffer buffer;
	Py_ssize_t size = -1;

	if (!PyArg_ParseTuple(args, "O|n", &pyobj, &size))
		return nullptr;

	if ( ! PyObject_CheckBuffer( pyobj ) ) {
		PyErr_SetString( PyExc_TypeError, "Value argument does not support the buffer interface" );
		return nullptr;
	}

	if ( PyObject_GetBuffer( pyobj, &buffer, PyBUF_SIMPLE ) != 0 ) {
		return nullptr;
	}

	if ( PyBuffer_IsContiguous( &buffer, 'A' ) == 0 ) {
        PyBuffer_Release(&buffer);
		PyErr_SetString(PyExc_NotImplementedError, "Support for non-contiguous buffers is currently not implemented");
		return nullptr;
	}

	// sanity check, should probably just deprecate the size argument
	if ( buffer.len > size ) {
		size = buffer.len;
	}

	if ( Write( buffer.buf, size ) != size ) {
		if ( ! PyErr_Occurred() ) {
			PyErr_SetString( PyExc_SystemError, "Internal error - writing buffer failed without further information" );
		}
        PyBuffer_Release(&buffer);
		return nullptr;
	}

	//if (!IsOpen())
	//	return NULL;

//	PyBufferProcs *buffer = pyobj->ob_type->tp_as_buffer;
//	if( !buffer || !buffer->bf_getreadbuffer )
//	{
//		BeOS->SetError(BEDEF, Clsid(), "Need a buffer or string as argument");
//		return nullptr;
//	}

//	Py_ssize_t segcount = buffer->bf_getsegcount(pyobj, 0);
//	for(Py_ssize_t i = 0; i<segcount; i++) {
//		const char* buff;
//		Py_ssize_t towrite = pyobj->ob_type->tp_as_buffer->bf_getreadbuffer(pyobj, 0, (void**)&buff);
//
//		if (towrite == -1)
//			return NULL;
//
//		if (size >= 0) {
//			if (towrite > size)
//				towrite = size;
//			size -= towrite;
//		}
//
//		if (Write(buff, towrite) != towrite)
//			return NULL;
//		if (!size)
//			break;
//	}

    PyBuffer_Release(&buffer);
	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
// IBlueStream::Seek
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::Pyseek(PyObject* args)
{
	return PySeek( args );
}

PyObject* IBlueStream_Thunk::PySeek(PyObject* args)
{
	int pos;
	int whence = 0;

	if( !PyArg_ParseTuple( args, "i|i", &pos, &whence ) )
	{
		return nullptr;
	}

	if( Seek( pos, (IBlueStream::SeekOrigin)whence ) == -1 )
	{
		return nullptr;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


#endif
