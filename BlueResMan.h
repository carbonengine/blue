#pragma once
#ifndef BLUERESMAN_H
#define BLUERESMAN_H

#include "include/Blue.h"
#include "include/IBlueResMan.h"
#include "CallbackMan.h"
#include "include/IBlueOS.h"
#include "MotherLode.h"

BLUE_DECLARE( BlueResMan );

// BlueResMan manages instances of BlueResource, ensuring we have unique
// objects for any given path. Calling GetResource for the first time with a given
// path creates an instance using a factory function selected from the file extension
// in the path. Subsequent calls to GetResource will return new references to the existing
// object.
class BlueResMan : 
	public IBlueResMan,
	public IWeakRef,
	public IBlueEvents
{
public:
	EXPOSE_TO_BLUE();

	BlueResMan( IRoot* lockobj = 0 );
	~BlueResMan();

	void Update();

	void Initialize();
	void Shutdown();

	typedef IBlueResource* (*CreateResourceFunction)( const wchar_t* path );
	static void RegisterFileExtension( const wchar_t* ext, CreateResourceFunction factory );

	void ResetQueueStats();
	void SetLoadingThreadCount( int n );

	Be::Result<std::string> GetResourceFromScript( const std::wstring& path, const std::wstring& ex, IRoot** resource );
	Be::Result<std::string> Wait();
	Be::Result<std::string> WaitUrgent();

	Be::Result<std::string> LoadObjectFromScript( const std::wstring& path, IRoot** obj );
	Be::Result<std::string> LoadObjectWithoutInitializeFromScript( const std::wstring& path, IRoot** obj );

	//////////////////////////////////////////////////////////////////////////
	// IBlueEvents
	void OnTick( Be::Time realTime, Be::Time simTime, void* cookie );

	//////////////////////////////////////////////////////////////////////////
	// IWeakRef
	//
	void WeakRefNotify( IWeakObject* p );

	//////////////////////////////////////////////////////////////////////////
	// IBlueResMan
	//
	bool GetResourceW( const std::wstring& path, const std::wstring& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr );
	bool GetResource( const std::string& path, const std::string& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr );

	// Returns true if the caller is on the same thread as the main thread queue
	bool IsOnMainThread();

	// Adds a callback request to any of the queues managed by the resource manager
	bool AddToQueue( BlueResManQueue q, IBlueCallbackMan::CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* id );
	
	// Cancels a previous callback request
	void CancelFromQueue( BlueResManQueue q, uint32_t id );

	// Gets the next id for the given queue. Useful to find out if anything was added
	// to a queue (get the id, do something, get the id again and compare).
	uint32_t GetNextIdForQueue( BlueResManQueue q );

	// Process the next item from the main thread queue - returns false if the queue was empty
	bool PumpMainThreadQueue();

	void PauseQueue( BlueResManQueue q );
	void ResumeQueue( BlueResManQueue q );

	// For debugging/timing - no resources are actually cleared, but resource
	// manager no longer knows about them so future requests will go out to disk.
	void ForgetAllResources();

	IRoot* LoadObject( const char* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE );
	IRoot* LoadObjectW( const wchar_t* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE );
	
	bool SaveObject( IRoot* obj, const char* name );
	bool SaveObjectW( IRoot* obj, const wchar_t* name );

	IRoot* GetObject( const std::string& path, const std::string& ex = "" );
	IRoot* GetObjectW( const std::wstring& path, const std::wstring& ex = L"" );
    void ClearCachedObject( const std::string& path, const std::string& ex = "" );
	void ClearCachedObjectW( const std::wstring& path, const std::wstring& ex = L"" );
    void ClearAllCachedObjects();

	void SetUrgentResourceLoads( bool b );
	bool IsUrgentResourceLoads();

#if BLUE_WITH_PYTHON
	void QueueCallbackForResourceLoads( PyObject* cb, PyObject* args );
	void QueueCallbackForUrgentResourceLoads( PyObject* cb, PyObject* args );
#endif

	// Notify manager of memory use. This function blocks if reserved memory exceeds
	// limits - other threads must release memory before calling thread is allowed
	// to continue.
	void ReserveBackgroundLoadMemory( size_t size );
	void ReleaseBackgroundLoadMemory( size_t size );

	unsigned int GetPendingLoads() const;
	unsigned int GetPendingPrepares() const;

	bool GetSubstituteBlackForRed() const;
	void SetSubstituteBlackForRed( bool val );

	void SetLoadingThreadPriority( int prio );

#if BLUE_WITH_PYTHON
	void RegisterResourceConstructor( const wchar_t* name, PyObject* constructor );
	void UnregisterResourceConstructor( const wchar_t* name );
#endif

private:
#if BLUE_WITH_PYTHON
	static PyObject* PyLoadObjectFromYamlString( PyObject* self, PyObject* args );
#endif

	IBlueResource* GetResourceHelper( const std::wstring& path, const std::wstring& ex, IBlueResManNotifications* notifications = nullptr );

	// If set, LoadObject attempts to load from a black file whenever red files are requested
	bool m_substituteBlackForRed;

	// If set, objects are looked up in the loadObject cache before reading from disk.
	// This is the default - can be disabled to simplify content generators lives.
	bool m_loadObjectCacheEnabled;

	// Cache for objects loaded via LoadObject
	PMotherLode m_loadObjectCache;

	// Time slice for LoadObject
	float m_loadObjectTimeSlice;

	// Should files be preloaded?
	bool m_loadObjectPreloadFiles;

	// This exists to allow us to load the same resource, but create different instances of it
	// allowing us to have different interior / exterior instances
	typedef std::pair<std::wstring, std::wstring> PathExtensionPairType;
	typedef TrackableStdMap<PathExtensionPairType, IWeakObject*> ObjectMap;
	ObjectMap m_objectInstances;

	typedef TrackableStdMap<IWeakObject*, PathExtensionPairType> ReverseObjectMap;
	ReverseObjectMap m_objectInstancesReverse;

	typedef	TrackableStdHashMap<std::wstring, IBlueResource*> ResourceMap;
	ResourceMap m_instances;

	bool m_urgentResourceLoads;

	CcpThreadId_t m_mainThread;

	IBlueCallbackManPtr m_threadQueues[BRMQ_COUNT];

	CcpMutex m_backgroundLoadMemoryMutex;

	size_t m_backgroundLoadMemoryBudget;
	size_t m_backgroundLoadMemoryInUse;

	unsigned int m_pendingLoads;
	unsigned int m_pendingPrepares;
	unsigned int m_preparesHandledLastTick;
	unsigned int m_preparesHandledPerTickMax;
	unsigned int m_preparesHandledTotal;

	float m_loadQueueTimeAverage;
	float m_loadQueueTimeMax;
	float m_prepareQueueTimeAverage;
	float m_prepareQueueTimeMax;

	float m_mainThreadTimeSlice;
	float m_mainThreadMaxTime;

#if BLUE_WITH_PYTHON
	typedef std::map<std::wstring, PyObject*> DynamicConstructors;
	DynamicConstructors m_dynamicConstructors;
#endif
};

TYPEDEF_BLUECLASS( BlueResMan );

#endif
