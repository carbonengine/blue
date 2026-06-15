// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "MotherLode.h"

BLUE_DEFINE_NO_REGISTER( MotherLode );
BLUE_REGISTER_CLASS_EX
( 
	MotherLode, 
	DynamicSingletonFactory<OMotherLode>::Create, 
	Be::ClassRegistration::DISABLE_PYTHON_CONSTRUCTION
);

const Be::ClassInfo* MotherLode::ExposeToBlue()
{
	EXPOSURE_BEGIN( MotherLode, "MotherLode is used to register and cache Blue objects and resources")
		MAP_INTERFACE(IMotherLode)

		MAP_PROPERTY
		( 
			"maxMemUsage", 
			GetCacheSize, SetCacheSize, 
			"Maximum memory consumption from cached objects. Every tick the cache"
			"\nis trimmed down to this size by clearing out objects in LRU order."
			"\nSet this to 0 to prevent caching from happening at all."
		)
		MAP_ATTRIBUTE
		( 
			"memUsage", 
			mMemUsage, 
			"Memory currently in use from cached objects."
			"\nNote that objects registered but not cached, i.e. objects that are"
			"\nstill referred to by objects other than the cache do not contribute"
			"\nthis number",
			Be::READ
		)
		MAP_ATTRIBUTE
		( 
			"verbose", 
			mVerbose, 
			"Set to True to get verbose logging", 
			Be::READWRITE
		)

		MAP_METHOD_AND_WRAP
		(
			"keys",
			GetKeys, 
			"Return all keys, both cached and non-cached. This is the same as"
			"calling GetCachedKeys and GetNonCachedKeys and combining the results."	
		)
		
		MAP_METHOD_AND_WRAP
		(
			"values",
			GetValues, 
			"return all values"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"items",
			Pyitems, 
			"return all items\n"
			":rtype: list[(unicode, IRoot)]"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"size",
			GetSize, 
			"get size"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"nWeak",
			PynWeak, 
			"get number of weak objects\n"
			":rtype: int"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"Insert",
			InsertFromScript, 
			"Insert a Blue object.\n"
			":param key: a (unicode) string\n"
			":param obj: the object stored under the key\n"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"Lookup",
			LookupFromScript, 
			"Look up an object.\n"
			":param key: a string to look up\n"
			":returns: Object stored under 'key', or None\n"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"LookupAsWeakRef",
			PyLookupAsWeakRef, 
			"look up an object, returning a weak reference to it if found\n"
			":param key: a string to look up\n"
			":type key: basestr\n"
			":rtype: BluePythonWeakRef"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"clear",
			Clear, 
			"flush everything"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"ClearCached",
			ClearCached,
			"Clears all cached objects from memory"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"Delete",
			DeleteFromScript, 
			"Delete a specific key.\n"
			":param key: a string, the key to delete"
			":returns: True if the key was found (and deleted), otherwise False"
		)
		
		MAP_METHOD_AS_METHOD
		( 
			"GetCachedKeys", 
			PyGetCachedKeys, 
			"Returns a list of keys that are cached. The objects behind those"
			"\nare owned by the cache - no one outside the cache itself references"
			"\nthem."
			"\n:rtype: list[unicode]"
		)
		MAP_METHOD_AS_METHOD
		( 
			"GetNonCachedKeys", 
			PyGetNonCachedKeys, 
			"Returns a list of registered keys that are not cached, that is other"
			"\nobjects hold a reference to them."
			"\n:rtype: list[unicode]"
		)
	EXPOSURE_END()
}