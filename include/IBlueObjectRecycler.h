// Copyright © 2012 CCP ehf.

#pragma once
#ifndef IBlueObjectRecycler_h
#define IBlueObjectRecycler_h

BLUE_INTERFACE( IBlueObjectRecycler ) : public IRoot
{
	// Get an instance of an object as loaded from 'resPath'. The first time this
	// is called for any given path, the object is simply loaded. Once that instance
	// is no longer in use, the recycler may decide to hang on to it for recycling
	// for later calls to GetObject with the same path.
	virtual bool RecycleOrLoad( const wchar_t* resPath, IRoot** obj ) = 0;

	// Recycles an object if possible, or makes a copy of the srcObj if no recyclable
	// instance exists. When instances copied are no longer in use, the recycler may
	// decide to hang on to them for recycling in later calls to this function with
	// the same key.
	virtual bool RecycleOrCopy( const wchar_t* key, IRoot* srcObj, IRoot** obj ) = 0;

	// Perform housekeeping, potentially releasing instances held onto for recycling
	// purposes.
	virtual void Update( Be::Time time ) = 0;

	// Clears any instances and history used to determine recycling policy.
	virtual void Clear() = 0;
};

extern IBlueObjectRecycler* BeRecycler;

#endif
