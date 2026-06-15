// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BLUERESMAN_H
#define BLUERESMAN_H

#include "IBluePersist.h"
#include "IBlueResMan.h"
#include "CallbackMan.h"
#include "IBlueOS.h"
#include "MotherLode.h"
#include "IRootReader.h"

BLUE_DECLARE( BlueResMan );

// BlueResMan manages instances of BlueResource, ensuring we have unique
// objects for any given path. Calling GetResource for the first time with a given
// path creates an instance using a factory function selected from the file extension
// in the path. Subsequent calls to GetResource will return new references to the existing
// object.
class BlueResMan : 
	public IBlueResMan,
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
	void OnTick( Be::Time realTime, Be::Time simTime, void* cookie ) override;

	//////////////////////////////////////////////////////////////////////////
	// IBlueResMan
	//
	bool GetResourceW( const std::wstring& path, const std::wstring& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr ) override;
	bool GetResource( const std::string& path, const std::string& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr ) override;

	// Returns true if the caller is on the same thread as the main thread queue
	bool IsOnMainThread() override;

	// Adds a callback request to any of the queues managed by the resource manager
	bool AddToQueue( BlueResManQueue q, IBlueCallbackMan::CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* id ) override;
	
	// Cancels a previous callback request
	void CancelFromQueue( BlueResManQueue q, uint32_t id ) override;

	// Gets the next id for the given queue. Useful to find out if anything was added
	// to a queue (get the id, do something, get the id again and compare).
	uint32_t GetNextIdForQueue( BlueResManQueue q ) override;

	// Process the next item from the main thread queue - returns false if the queue was empty
	bool PumpMainThreadQueue() override;

	void PauseQueue( BlueResManQueue q ) override;
	void ResumeQueue( BlueResManQueue q ) override;

	// For debugging/timing - no resources are actually cleared, but resource
	// manager no longer knows about them so future requests will go out to disk.
	void ForgetAllResources();

	IRootPtr LoadObject( const char* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE ) override;
	IRootPtr LoadObject( const wchar_t* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE ) override;

	bool SaveObject( IRoot* obj, const char* name ) override;
	bool SaveObjectW( IRoot* obj, const wchar_t* name ) override;

	void SetUrgentResourceLoads( bool b ) override;
	bool IsUrgentResourceLoads() override;

	// Notify manager of memory use. This function blocks if reserved memory exceeds
	// limits - other threads must release memory before calling thread is allowed
	// to continue.
	void ReserveBackgroundLoadMemory( size_t size ) override;
	void ReleaseBackgroundLoadMemory( size_t size ) override;

	unsigned int GetPendingLoads() const override;
	unsigned int GetPendingPrepares() const override;

	void SetLoadingThreadPriority( int prio );

	void RegisterResourceConstructor( const wchar_t* name, IBlueDynamicResourceConstructor* constructor ) override;
	void RegisterScriptResourceConstructor( const wchar_t* name, BlueScriptCallback constructor );
	void UnregisterResourceConstructor( const wchar_t* name ) override;

private:
#if BLUE_WITH_PYTHON
	static PyObject* PyLoadObjectFromYamlString( PyObject* self, PyObject* args );
#endif

	Be::Result<std::string> GetFileContentsWithYield( std::wstring filename, IBlueStreamPtr& sourceStream );
	std::set<std::wstring> m_filesInProgress;

	IBlueResource* GetResourceHelper( const std::wstring& path, const std::wstring& ex, IBlueResManNotifications* notifications = nullptr );

	void GetReaderForStream( std::wstring filename, IBlueStream* sourceStream, IRootReaderPtr& reader );

	// If set, objects are looked up in the loadObject cache before reading from disk.
	// This is the default - can be disabled to simplify content generators lives.
	bool m_loadObjectCacheEnabled;

	// Cache for objects loaded via LoadObject
	PMotherLode m_loadObjectCache;

	// Time slice for LoadObject
	float m_loadObjectTimeSlice;

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
	unsigned int m_maxAllowedInPrepareQueue;

	float m_loadQueueTimeAverage;
	float m_loadQueueTimeMax;
	float m_prepareQueueTimeAverage;
	float m_prepareQueueTimeMax;

	float m_mainThreadTimeSlice;
	float m_mainThreadMaxTime;

	typedef std::map<std::wstring, std::unique_ptr<IBlueDynamicResourceConstructor>> DynamicConstructors;
	DynamicConstructors m_dynamicConstructors;
};

TYPEDEF_BLUECLASS( BlueResMan );


class BluePythonDynamicResourceConstructor : public IBlueDynamicResourceConstructor
{
public:
	explicit BluePythonDynamicResourceConstructor( BlueScriptCallback callable );

	IBlueResource* GetResource( const wchar_t* query );
private:
	BlueScriptCallback m_callable;
};

BLUEIMPORT IBlueResMan* GetBeResMan();

#endif
