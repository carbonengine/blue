// Copyright © 2008 CCP ehf.

/* 
	*************************************************************************

	IMotherLode.h

	Project:   Blue

	Description:   

		Interface for the MotherLode object

	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _IMOTHERLODE_H_
#define _IMOTHERLODE_H_

#include "ICacheable.h"

BLUE_INTERFACE(IMotherLode) : public IRoot
{
	// -------------------------------------------------------------
	// Description:
	//   Caching behaviour for individual resource. This allows 
	//   disabling caching for resource even if its class supports
	//   caching.
	// -------------------------------------------------------------
	enum ResourceCaching
	{
		// Disable caching for resource
		CACHING_NOT_ALLOWED,

		// Allow caching (if it is supported by resource)
		CACHING_ALLOWED,
	};

	enum LookupResult
	{
		// Lookup failed
		LOOKUP_FAILED,

		// Lookup succeeded
		LOOKUP_SUCCESS,

		// Lookup succeeded - object was in cache
		LOOKUP_CACHED
	};

	// Insert the object in the cache.  Returns true on success, false if
	// an error occurred, such as a QueryInterface problem. (the objects must
	// support the IWeakObject interface)
	// If "replace" is false, *inserted can be inspected to see if the
	// insert succeeded or if another object with the same key was present.
	virtual bool Insert(
		const wchar_t*	key,							//the key to assign this to
		IRoot*			object,							//the object to store
		bool			replace = true,					//if true, replace any object found with the same key 
		bool			*inserted = 0,					//Was the object inserted?
		ResourceCaching	allowCaching = CACHING_ALLOWED	//allow caching of this resource
		) = 0;

	virtual bool HasKey( const wchar_t* key ) = 0;

	// Lookup from the cache.
	// *ppv should be 0 on entry.
	// If successful, returns true and non-zero in the *ppv pointer.
	// If object is not found, returns true, and zero in the *ppv pointer.
	// If there is an error, e.g. an QueryInterface failure, return LOOKUP_FAILURE,
	// otherwise LOOKUP_CACHED is returned if object was in cache or LOOKUP_SUCCESS
	// if object is registered an not cached (shared).
	virtual LookupResult Lookup(
		const wchar_t *key,				//The key for where the file is found
		const Be::IID& riid,			//The interface to get
		void** ppv,						//pointer to result pointer
		BLUEQIOPT options = BEQI_NONE	//How to QI for the pointer
		) = 0;

	// Manage the size of cache
	virtual void SetCacheSize(size_t mem) = 0;
	virtual size_t GetCacheSize() = 0;
	virtual void GetStats(size_t *n_live, size_t *n_cached, size_t *c_mem) = 0;

	//Keep house
	virtual void Startup() = 0;
	virtual void Shutdown() = 0;

	// Delete a specific key from the lookup table.  Returns true if the key was found,
	// false otherwise.
	virtual bool Delete(const wchar_t *key) = 0;
};

extern BLUEIMPORT IMotherLode* BeMotherLode;

#endif // !defined _IMOTHERLODE_H_
