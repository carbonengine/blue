// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "DictReader.h"

BLUE_DEFINE( DictReader );

namespace
{

PyObject* PyCreateObject( PyObject* self, PyObject* args )
{
	DictReader* pThis = BluePythonCast<DictReader*>( self );

	PyObject* dict = nullptr;
	if( !PyArg_ParseTuple( args, "O", &dict ) )
	{
		return nullptr;
	}

	try
	{
		IRoot* res = pThis->CreateObject( dict );
		if( res )
		{
			PyObject* wrappedRes = BlueWrapObjectForPython( res );
			res->Unlock();
			return wrappedRes;
		}
		else
		{
			Py_RETURN_NONE;
		}

	}
	catch( std::exception& e )
	{
		std::string msg = "Error while creating object from dict:\n";
		msg += e.what();
		PyErr_SetString( PyExc_RuntimeError, msg.c_str() );
		return nullptr;
	}
}

}

const Be::ClassInfo* DictReader::ExposeToBlue()
{
	EXPOSURE_BEGIN( DictReader, "DictReader constructs Blue objects from a dict." )
		MAP_INTERFACE( DictReader )

		MAP_ATTRIBUTE
		(
			"doInitialize",
			m_doInitialize,
			"If set (the default), then objects are initialized in CreateObject.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"persistedAttributesOnly",
			m_persistedAttributesOnly,
			"If set (the default), then only persisted attributes can be set from the\n"
			"when creating objects. Clearing this flag allows any writable attribute\n"
			"to be set. Note that attributes that are mapped as properties (have\n"
			"getters/setters in C++ code can not currently be set.",
			Be::READWRITE
		)
		
		MAP_METHOD
		(
			"CreateObject",
			PyCreateObject,
			"Creates a Blue object from a description in the given dict.\n"
			":param desc: object description\n"
			":type desc: dict\n"
			":rtype: IRoot"
		)
	EXPOSURE_END()
}

#endif
