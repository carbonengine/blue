#include "StdAfx.h"

#include "BlueResMan.h"
#include "include/IBlueResource.h"
#include "BlueMemStream.h"
#include "YamlWriter.h"

#if BLUE_WITH_PYTHON
static PyObject* PySaveObject( PyObject* self, PyObject* args )
{
	PyObject* pyObj;
	PyObject* nameO;

	if( !PyArg_ParseTuple( args, "OO", &pyObj, &nameO ) )
	{
		return NULL;
	}
	BluePy u(PyUnicode_FromObject(nameO));
	if( !u )
	{
		return 0;
	}
	const wchar_t *name = (const wchar_t*)PyUnicode_AS_UNICODE(u.o);

	IRootPtr obj( BlueUnwrapObjectFromPython(pyObj) );

	if( !obj )
	{
		PyErr_SetString( PyExc_TypeError, "The first argument is not an object");
		return NULL;
	}

	if( BeResMan->SaveObjectW( obj, name ) )
	{
		Py_INCREF( Py_True );
		return Py_True;
	}
	else
	{
		Py_INCREF( Py_False );
		return Py_False;
	}
}

static PyObject* PySaveObjectToYamlString( PyObject* self, PyObject* args )
{
	PyObject* pyObj;

	if( !PyArg_ParseTuple( args, "O", &pyObj ) )
	{
		return NULL;
	}

	IRootPtr obj( BlueUnwrapObjectFromPython(pyObj) );

	if( !obj )
	{
		PyErr_SetString( PyExc_TypeError, "The first argument is not an object");
		return NULL;
	}

	CMemStream ms;
	CYamlWriter writer;

	writer.WriteObjectToStream( obj, &ms );

	ssize_t size = ms.GetSize();
	PyObject* returnValue = PyString_FromStringAndSize( nullptr, size );

	void* data;
	ms.LockData( &data, size );

	char* dst = PyString_AsString( returnValue );
	memcpy( dst, data, size );

	ms.UnlockData();

	return returnValue;
}

static PyObject* PyRegisterResourceConstructor( PyObject* self, PyObject* args )
{
	BlueResMan* pThis = BluePythonCast<BlueResMan*>( self );
	CCP_ASSERT( pThis == BeResMan );

	PyObject *nameObject = NULL;
	PyObject *constructor = NULL;
	if (!PyArg_ParseTuple(args, "OO", &nameObject, &constructor ))
	{
		return NULL;
	}

	if( !PyString_Check( nameObject ) )
	{
		return NULL;
	}

	std::wstring name = (const wchar_t*)CA2W( PyString_AsString( nameObject ) );

	pThis->RegisterResourceConstructor( name.c_str(), constructor );

	Py_RETURN_NONE;
}

static PyObject* PyRegisterResourceConstructorW( PyObject* self, PyObject* args )
{
	BlueResMan* pThis = BluePythonCast<BlueResMan*>( self );
	CCP_ASSERT( pThis == BeResMan );

	PyObject *nameObject = NULL;
	PyObject *constructor = NULL;
	if (!PyArg_ParseTuple(args, "OO", &nameObject, &constructor ))
	{
		return NULL;
	}

	if( !PyString_Check( nameObject ) )
	{
		return NULL;
	}

	std::wstring name = (const wchar_t*)PyUnicode_AsUnicode( nameObject );

	pThis->RegisterResourceConstructor( name.c_str(), constructor );

	Py_RETURN_NONE;
}

static PyObject* PyUnregisterResourceConstructor( PyObject* self, PyObject* args )
{
	BlueResMan* pThis = BluePythonCast<BlueResMan*>( self );
	CCP_ASSERT( pThis == BeResMan );

	PyObject *nameObject = NULL;
	if (!PyArg_ParseTuple(args, "O", &nameObject ))
	{
		return NULL;
	}

	if( !PyString_Check( nameObject ) )
	{
		return NULL;
	}

	std::wstring name = (const wchar_t*)CA2W( PyString_AsString( nameObject ) );

	pThis->UnregisterResourceConstructor( name.c_str() );

	Py_RETURN_NONE;
}

static PyObject* PyUnregisterResourceConstructorW( PyObject* self, PyObject* args )
{
	BlueResMan* pThis = BluePythonCast<BlueResMan*>( self );
	CCP_ASSERT( pThis == BeResMan );

	PyObject *nameObject = NULL;
	if (!PyArg_ParseTuple(args, "O", &nameObject ))
	{
		return NULL;
	}

	if( !PyString_Check( nameObject ) )
	{
		return NULL;
	}

	std::wstring name = (const wchar_t*)PyUnicode_AsUnicode( nameObject );

	pThis->UnregisterResourceConstructor( name.c_str() );

	Py_RETURN_NONE;
}
#endif

const Be::ClassInfo* BlueResMan::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResMan, "" )
		MAP_INTERFACE( IBlueResMan )

		MAP_ATTRIBUTE
		(
			"loadObjectCacheEnabled",
			m_loadObjectCacheEnabled,
			"If set, objects are looked up in the loadObject cache before reading from disk.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"loadObjectTimeSlice",
			m_loadObjectTimeSlice,
			"Time slice for calls to LoadObject. Once the time slice is used up\n"
			"the tasklet yields and continues on the next frame.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"loadObjectCache", 
			m_loadObjectCache, 
			"Cache for objects loaded via LoadObject", 
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"mainThreadTimeSlice", 
			m_mainThreadTimeSlice, 
			"Time allowed for callbacks on the main thread queue. Note that individual callbacks\n"
			"are never interrupted, but if accumulated time in processing callbacks exceeds this\n"
			"value, further callbacks are delayed until next frame.", 
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"mainThreadMaxTime", 
			m_mainThreadMaxTime, 
			"Maximum time taken for processing callbacks on the main thread queue. This can exceed\n"
			"the allowed time slice as individual callbacks are never interrupted.", 
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"backgroundLoadMemoryBudget", 
			m_backgroundLoadMemoryBudget, 
			"Maximum memory budget allowed for asynchronously loaded resources. Once load requests\n"
			"consume more than this budget, loading is stalled until preparation finishes and memory\n"
			"used while loading in the background has been released.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"backgroundLoadMemoryInUse", 
			m_backgroundLoadMemoryInUse, 
			"Memory currently in use for loading resources in the background. Requests to reserve\n"
			"memory beyond 'backgroundLoadMemoryBudget' stall the loading progress until memory\n"
			"has been released.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		( 
			"pendingLoads", 
			m_pendingLoads, 
			"Count of items in background load queue", 
			Be::READ
		)
		
		MAP_ATTRIBUTE
		( 
			"pendingPrepares", 
			m_pendingPrepares, 
			"Count of items in main thread queue", 
			Be::READ
		)
		
		MAP_ATTRIBUTE
		( 
			"preparesHandledLastTick", 
			m_preparesHandledLastTick, 
			"Count of items handled from the main thread queue on the last tick", 
			Be::READ
		)

		MAP_ATTRIBUTE
		( 
			"preparesHandledPerTickMax", 
			m_preparesHandledPerTickMax, 
			"Maximum number of items handled from the main thread queue in one tick", 
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"preparesHandledTotal", 
			m_preparesHandledTotal, 
			"Count of items handled from the main thread queue since the program started", 
			Be::READ
		)

		MAP_ATTRIBUTE
		( 
			"maxAllowedInPrepareQueue", 
			m_maxAllowedInPrepareQueue, 
			"Maximum allowed in prepare queue - if too many items accumulate in the queue\n"
			"the mainThreadTimeSlice value is ignored to prevent pathological buildup of\n"
			"prepare items.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		( 
			"loadQueueTimeAverage", 
			m_loadQueueTimeAverage, 
			"Average time for entries waiting in the load queue", 
			Be::READ
		)

		MAP_ATTRIBUTE
		( 
			"loadQueueTimeMax", 
			m_loadQueueTimeMax, 
			"Maximum time for entries waiting in the load queue", 
			Be::READ
		)

		MAP_ATTRIBUTE
		( 
			"prepareQueueTimeAverage", 
			m_prepareQueueTimeAverage, 
			"Average time for entries waiting in the prepare queue", 
			Be::READ
		)

		MAP_ATTRIBUTE
		( 
			"prepareQueueTimeMax", 
			m_prepareQueueTimeMax, 
			"Maximum time for entries waiting in the prepare queue", 
			Be::READ
		)

		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS
		( 
			"GetResource", 
			GetResourceFromScript,
			1,
			"Get a resource associated with the given path name\n"
			":param path: res path to resource file\n"
			":param ex: additional extension"
		)
		
		// TODO: Remove this - string parameters are now automatically promoted
		MAP_METHOD_AND_WRAP
		( 
			"GetResourceW", 
			GetResourceFromScript, 
			"Get a resource associated with the given path name\n"
			":param path: res path to resource file\n"
			":param ex: additional extension"
		)

		MAP_METHOD_AND_WRAP
		(
			"LoadObject",
			LoadObjectFromScript,
			"Loads object from a file (.blue or .red based on extension).\n"
			"This function may yield the calling tasklet while disk io takes place\n"
			":param filename: path to the file to load"
		)

		MAP_METHOD
		(
			"SaveObject",
			PySaveObject,
			"Saves an object to a file (.red or .blue based on extension). Returns the success of operation.\n"
			":param obj: object to save\n"
			":type obj: IRoot\n"
			":param filename: path to the file to save to\n"
			":type filename: basestring\n"
			":rtype: bool"
		)
		
		MAP_METHOD
		(
			"SaveObjectToYamlString",
			PySaveObjectToYamlString,
			"Saves an object to yaml representation in a string. The string\n"
			"is equivalent to the contents of a red file saved with SaveObject.\n"
			":param obj: object to save\n"
			":type obj: IRoot\n"
			":rtype: str"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"LoadObjectWithoutInitialize",
			LoadObjectWithoutInitializeFromScript,
			"Loads object from a file (.blue or .red based on extension).\n"
			"The object is not initialized - see LoadObject for regular loading.\n"
			"This can be useful when all that is needed is to inspect the structure\n"
			"of the object without using it in any way.\n"
			"This function may yield the calling tasklet while disk io takes place\n"
			":param filename: path to the file to load\n"
		)

		MAP_METHOD
		(
			"LoadObjectFromYamlString",
			PyLoadObjectFromYamlString, 
			"Loads object from a string that has yaml contents\n"
			":param yaml: YAML\n"
			":type yaml: str\n"
			":rtype: None | IRoot"
		)

		MAP_METHOD_AND_WRAP
		( 
			"Wait", 
			Wait, 
			"Waits for currently scheduled resource loads to finish"
		)
		
		MAP_METHOD_AND_WRAP
		( 
			"WaitUrgent", 
			WaitUrgent, 
			"Waits for currently scheduled urgent resource loads to finish"
		)

		MAP_METHOD_AND_WRAP( 
			"SetUrgentResourceLoads", 
			SetUrgentResourceLoads, 
			"Enables (or disables) urgent resource loads\n"
			":param enable: " )
		MAP_METHOD_AND_WRAP( "ResetQueueStats", ResetQueueStats, "Resets stats for load and prep queues" )

		MAP_METHOD_AND_WRAP
		(
			"SetLoadingThreadPriority",
			SetLoadingThreadPriority,
			"Sets the priority of the loading thread(s).\n"
			":param prio: positive for higher than normal, negative for lower priority\n"
		)
		MAP_METHOD_AND_WRAP
		(
			"SetLoadingThreadCount",
			SetLoadingThreadCount,
			"Sets the count of loading threads.\n"
			":param count: usually lower or equal to number of cores available, not lower than 1."
		)

		MAP_METHOD( 
			"RegisterResourceConstructor", 
			PyRegisterResourceConstructor, 
			"Registers a new dynamic resource constructor function. This function is called \n" 
			"for resources with paths like \"dynamic:/name\". If the constructor with the \n"
			"same name is already registered, the new function will override the old one.\n"
			":param name: Name to associate with the constructor in resource paths\n"
			":type name: str\n"
			":param function: A callable object that is used to construct dynamic resources\n"
			":type function: (unicode)->IRoot\n"
			":rtype: None"
			)

		MAP_METHOD( 
			"RegisterResourceConstructorW", 
			PyRegisterResourceConstructorW, 
			"Registers a new dynamic resource constructor function. This function is called \n" 
			"for resources with paths like \"dynamic:/name\". If the constructor with the \n"
			"same name is already registered, the new function will override the old one.\n"
			":param name: Name to associate with the constructor in resource paths\n"
			":type name: unicode\n"
			":param function: A callable object that is used to construct dynamic resources\n"
			":type function: (unicode)->IRoot\n"
			":rtype: None"
			)

		MAP_METHOD( 
			"UnregisterResourceConstructor", 
			PyUnregisterResourceConstructor, 
			"Unregisters dynamic resource constructor function previously registered with a " 
			"call to RegisterResourceConstructor.\n"
			":param name: Name associated with the constructor function to unregister\n"
			":type name: str\n"
			":rtype: None"
			)

		MAP_METHOD( 
			"UnregisterResourceConstructorW", 
			PyUnregisterResourceConstructorW, 
			"Unregisters dynamic resource constructor function previously registered with a " 
			"call to RegisterResourceConstructor.\n"
			":param name: Name associated with the constructor function to unregister\n"
			":type name: unicode\n"
			":rtype: None"
			)

		EXPOSURE_END()
}
