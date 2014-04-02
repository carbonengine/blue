////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		January 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "include/IBluePersist.h"
#include "BlueExposure/include/BlueExposureMacrosDeprecated.h"
#include "Include/IBlueOS.h"

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
			MAPPYTHON( Write,	"Write" )
			MAPPYTHON( Seek,	"Seek" )
		THUNKER_END()
	}

	DECLARE_PYMETHODTHUNK( Read );
	DECLARE_PYMETHODTHUNK( Write );
	DECLARE_PYMETHODTHUNK( Seek );
};

BLUE_REGISTER_THUNKER(IBlueStream_Thunk::Defs(), IBlueStream_Thunk::IID());

//--------------------------------------------------------------------
// IBlueStream::Read
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::PyRead(PyObject* args)
{
	Py_ssize_t size = -1;

	if (!PyArg_ParseTuple(args, "|n", &size))
		return NULL;

	//if (!IsOpen())
	//return NULL;

	if (size < 0)
		size = GetSize() - GetPosition();

	PyObject* str = PyString_FromStringAndSize(0, size);
	if (!str)
		return NULL;
	if (size) {
		ssize_t read = Read(PyString_AS_STRING(str), size);
		if (read < 0) {
			Py_DECREF(str);
			return NULL;
		}
		if (read != size && _PyString_Resize(&str, read))
			return 0;
	}
	return str;
}


//--------------------------------------------------------------------
// IBlueStream::Write
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::PyWrite(PyObject* args)
{
	PyObject* pyobj;
	Py_ssize_t size = -1;

	if (!PyArg_ParseTuple(args, "O|n", &pyobj, &size))
		return NULL;

	//if (!IsOpen())
	//	return NULL;

	PyBufferProcs *buffer = pyobj->ob_type->tp_as_buffer;
	if( !buffer || !buffer->bf_getreadbuffer )
	{
		BeOS->SetError(BEDEF, Clsid(), "Need a buffer or string as argument");
		return nullptr;
	}

	Py_ssize_t segcount = buffer->bf_getsegcount(pyobj, 0);
	for(Py_ssize_t i = 0; i<segcount; i++) {
		const char* buff;
		Py_ssize_t towrite = pyobj->ob_type->tp_as_buffer->bf_getreadbuffer(pyobj, 0, (void**)&buff);

		if (towrite == -1)
			return NULL;

		if (size >= 0) {
			if (towrite > size)
				towrite = size;
			size -= towrite;
		}

		if (Write(buff, towrite) != towrite)
			return NULL;
		if (!size)
			break;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
// IBlueStream::Seek
//--------------------------------------------------------------------
PyObject* IBlueStream_Thunk::PySeek(PyObject* args)
{
	int pos;

	if (!PyArg_ParseTuple(args, "i", &pos))
		return NULL;

	//if (!IsOpen())
	//	return NULL;

	if (Seek(pos, SO_BEGIN) == -1)
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


#endif
