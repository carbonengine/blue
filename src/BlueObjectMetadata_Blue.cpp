// Copyright © 2017 CCP ehf.

#include "StdAfx.h"
#include "BlueObjectMetadata.h"


BLUE_DEFINE_INTERFACE( IBlueObjectMetadata );

BLUE_DEFINE( BlueObjectMetadata );

const Be::ClassInfo* BlueObjectMetadata::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueObjectMetadata, "" )
		MAP_INTERFACE( BlueObjectMetadata )
		MAP_INTERFACE( IBlueObjectMetadata )

		MAP_METHOD_AND_WRAP( 
			"Set", 
			Set, 
			"Assigns a new value to a metadata key for an object\n"
			":param obj: owner object\n"
			":param key: metadata key\n"
			":param value: metadata value\n"
		)
		MAP_METHOD_AND_WRAP( 
			"Get", 
			Get, 
			"Returns a matadata for a given object and key\n" 
			":param obj: owner object\n"
			":param key: metadata key\n"
			":param default: default value to return if an object or a key is not found\n"
		)
		MAP_METHOD_AND_WRAP( 
			"Delete", 
			Delete, 
			"Deletes a specified metadata key for a given object\n" 
			":param obj: owner object\n"
			":param key: metadata key\n"
			":raises KeyError: if the object or key is not found"
		)
		MAP_METHOD_AND_WRAP( 
			"DeleteObject", 
			DeleteObject, 
			"Deletes all metadata for specified object\n"
			":param obj: owner object\n"
			":raises KeyError: if the object is not found"
		)
		MAP_METHOD_AND_WRAP( 
			"Index", 
			Index, 
			"Returns a matadata for a given object and key\n"
			":param obj: owner object\n"
			":param key: metadata key\n"
			":raises KeyError: if the object or key is not found"
		)
		MAP_METHOD_AND_WRAP( 
			"GetKeys", 
			GetKeys, 
			"Returns all metadata keys for a given object\n"
			":param obj: owner object\n"
			":raises KeyError: if the object is not found"
		)
		MAP_METHOD_AND_WRAP( 
			"GetItems", 
			GetItems, 
			"Returns all metadata as key->value dict for a given object\n"
			":param obj: owner object\n"
			":raises KeyError: if the object is not found"
		)
		MAP_METHOD_AND_WRAP( 
			"CopyShallow", 
			CopyShallow,
			"Copies the metadata for one object - excluding it's children - to another.\n"
			":param source: owner object to copy metadata from.\n"
			":param target: owner object to copy metadata to.\n"
		)
		MAP_METHOD_AND_WRAP( 
			"CopyDeep", 
			CopyDeep,
			"Copies all the metadata for one object - and it's children - to another.\n"
			":param source: owner object to copy metadata from.\n"
			":param target: owner object to copy metadata to.\n"
		)

	EXPOSURE_END()
}

MAP_FUNCTION_AND_WRAP( 
	"CopyWithMetadata", 
	PyCopyWithMetadata, 
	"Copies an item, along with any associated metadata.\n"
	":param src: the item to copy.\n"
	":type src: IRoot\n"
	":returns: a copy of the src item.\n"
	":rtype: IRoot" 
);