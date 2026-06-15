// Copyright © 2012 CCP ehf.

#pragma once
#ifndef	BlueObjectRecycler_h
#define BlueObjectRecycler_h

#include "IBlueObjectRecycler.h"

BLUE_CLASS( BlueObjectRecycler ) : 
	public IBlueObjectRecycler, 
	public IWeakRef
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectRecycler( IRoot* lockobj = nullptr );
	~BlueObjectRecycler();

#if BLUE_WITH_PYTHON
	// Returns information about the state of the recycler.
	// Returns a list of tuples, where each tuple holds:
	// - path
	// - number of requests for the path
	// - number of live objects
	// - maximum number of live objects
	// - recycled count
	// - number of instances ready to be recycled
	PyObject* GetInfo();
#endif

	
	//////////////////////////////////////////////////////////////////////////
	// IBlueObjectRecycler

	// Get an instance of an object as loaded from 'resPath'. The first time this
	// is called for any given path, the object is simply loaded. Once that instance
	// is no longer in use, the recycler may decide to hang on to it for recycling
	// for later calls to GetObject with the same path.
	bool RecycleOrLoad( const wchar_t* resPath, IRoot** obj ) override;

	// Recycles an object if possible, or makes a copy of the srcObj if no recyclable
	// instance exists. When instances copied are no longer in use, the recycler may
	// decide to hang on to them for recycling in later calls to this function with
	// the same key.
	bool RecycleOrCopy( const wchar_t* key, IRoot* srcObj, IRoot** obj ) override;

	// Perform housekeeping, potentially releasing instances held onto for recycling
	// purposes.
	void Update( Be::Time time ) override;

	// Clears any instances and history used to determine recycling policy.
	void Clear() override;

	//////////////////////////////////////////////////////////////////////////
	// IWeakRef
	void WeakRefNotify( IWeakObject *ptr ) override;
	//////////////////////////////////////////////////////////////////////////

private:
	struct ObjectInfo
	{
		std::wstring resPath;
		uint32_t totalRequests;
		uint32_t liveCount;
		uint32_t maxLiveCount;
		uint32_t recycledCount;
		std::vector<IRoot*> instances;
		Be::Time timeOfLastRequest;
	};

	void Add( const std::wstring& key, IRoot* instance );
	IRoot* Recycle( const std::wstring& key );

	TrackableStdMap<std::wstring, ObjectInfo*> m_objectInfoByName;
	TrackableStdMap<IWeakObject*, ObjectInfo*> m_objectInfoByObject;
	TrackableStdSet<ObjectInfo*> m_objectInfosWithInstances;

	// Time limit, in seconds, for recycling policy
	float m_timeLimit;
};

TYPEDEF_BLUECLASS( BlueObjectRecycler );

#endif
