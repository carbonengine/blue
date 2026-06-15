// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#include "BlueObjectRecycler.h"

BLUE_DEFINE( BlueObjectRecycler );

#if BLUE_WITH_PYTHON

namespace 
{
	PyObject* PyRecycleOrLoad( PyObject* self, PyObject* args )
	{
		BlueObjectRecycler* pThis = BluePythonCast<BlueObjectRecycler*>( self );

		PyObject* pathO;
		if( !PyArg_ParseTuple( args, "O", &pathO ) )
		{
			return nullptr;
		}

		std::wstring path;
		if( !BlueExtractWString( pathO, path ) )
		{
			return nullptr;
		}

		IRootPtr obj;
		if( !pThis->RecycleOrLoad( path.c_str(), &obj ) )
		{
			if( !PyErr_Occurred() )
			{
				PyErr_SetString( PyExc_RuntimeError, "Couldn't get object" );
			}
			return nullptr;
		}

		return BlueWrapObjectForPython( obj );
	}


	PyObject* PyRecycleOrCopy( PyObject* self, PyObject* args )
	{
		BlueObjectRecycler* pThis = BluePythonCast<BlueObjectRecycler*>( self );

		PyObject* pathO;
		PyObject* srcO;
		if( !PyArg_ParseTuple( args, "OO", &pathO, &srcO ) )
		{
			return nullptr;
		}

		std::wstring path;
		if( !BlueExtractWString( pathO, path ) )
		{
			return nullptr;
		}

		IRootPtr src = BluePythonCast<IRoot*>( srcO );

		IRootPtr obj;
		if( !pThis->RecycleOrCopy( path.c_str(), src, &obj ) )
		{
			PyErr_SetString( PyExc_RuntimeError, "Couldn't get object" );
			return nullptr;
		}

		return BlueWrapObjectForPython( obj );
	}
}

#endif

const Be::ClassInfo* BlueObjectRecycler::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueObjectRecycler, "BlueClasses handles class registration" )
		MAP_INTERFACE( IBlueObjectRecycler )
		MAP_INTERFACE( BlueObjectRecycler )

#if BLUE_WITH_PYTHON
		MAP_METHOD_AND_WRAP
		(
			"GetInfo",
			GetInfo,
			"Returns information about the state of the recycler.\n"
			"Returns a list of tuples, where each tuple holds:\n"
			" - path\n"
			" - number of requests for the path\n"
			" - number of live objects\n"
			" - maximum number of live objects\n"
			" - number of instances ready to be recycled"
		)
#endif

		MAP_METHOD
		(
			"RecycleOrLoad",
			PyRecycleOrLoad,
			"Get an instance of an object as loaded from the given path. The first time this\n"
			"is called for any given path, the object is simply loaded. Once that instance\n"
			"is no longer in use, the recycler may decide to hang on to it for recycling\n"
			"for later calls to RecycleOrLoad with the same path.\n\n"
			":param resPath: res path to load\n"
			":type resPath: basestring\n"
			":rtype: IRoot"
		)

		MAP_METHOD
		(
			"RecycleOrCopy",
			PyRecycleOrCopy,
			"Recycles an object if possible, or makes a copy of the srcObj if no recyclable\n"
			"instance exists. When instances copied are no longer in use, the recycler may\n"
			"decide to hang on to them for recycling in later calls to this function with\n"
			"the same key.\n"
			":param resPath: res path\n"
			":type resPath: basestring\n"
			":param srcObj: example object\n"
			":type srcObj: IRoot\n"
			":rtype: IRoot"
		)

		MAP_METHOD_AND_WRAP
		(
			"Update",
			Update,
			"Perform housekeeping, potentially releasing instances held onto for recycling purposes.\n"
			":param time: current time"
		)

		MAP_METHOD_AND_WRAP
		(
			"Clear",
			Clear,
			"Clear any recyclable instances as well as history used for determining\n"
			"recycling policy."
		)

		MAP_ATTRIBUTE
		(
			"timeLimit",
			m_timeLimit,
			"Time limit, in seconds, for recycling policy.",
			Be::READWRITE
		)
	EXPOSURE_END()
}
