#pragma once

#ifndef IBLUERESMAN_H
#define IBLUERESMAN_H

#include "IBlueCallbackMan.h"
#include "CcpCore/include/CcpAtomic.h"
#include <string>

enum BlueResManQueue
{
	BRMQ_MAIN,
	BRMQ_BACKGROUND,

	BRMQ_COUNT
};

// Optional callbacks that clients can use to get more detailed notifications of what's
// going on inside BlueResMan::GetResource.
// All callbacks are guaranteed to be made during the (blocking) call to GetResource on the
// main thread, so temporary lifetime objects that implement the callbacks are valid.
struct IBlueResManNotifications
{
	// Called by GetResource when a resource was not found in the cache, and is newly created.
	// At the time of callback, the class instance has been created, but not yet Initialize()'d.
	virtual void OnResourceCreated( void* ) {}

	// Called by GetResource when a resource is found in the cache.
	virtual void OnResourceFromCache( void* ) {}

protected:
	~IBlueResManNotifications() {}
};

struct IBlueResource;

BLUE_INTERFACE( IBlueResMan ) : public IRoot
{
	// Get a reference to the resource identified by path+ex, with the given interface
	virtual bool GetResourceW( const std::wstring& path, const std::wstring& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr ) = 0;
	virtual bool GetResource( const std::string& path, const std::string& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications = nullptr ) = 0;

	template <typename T>
	bool GetResource( const std::string& path, const std::string& ex, T*& resource, IBlueResManNotifications* notifications = nullptr )
	{
		return GetResource( path, ex, BlueInterfaceIID<T>(), (void**)&resource, notifications );
	}

	template <typename T>
	bool GetResource( const std::string& path, const std::string& ex, BluePtr<T>& resource, IBlueResManNotifications* notifications = nullptr )
	{
		resource.Unlock();
		return GetResource( path, ex, BlueInterfaceIID<T>(), (void**)&resource, notifications );
	}

	template <typename T>
	bool GetResource( const std::wstring& path, const std::wstring& ex, T*& resource, IBlueResManNotifications* notifications = nullptr )
	{
		return GetResource( path, ex, BlueInterfaceIID<T>(), (void**)&resource, notifications );
	}

	template <typename T>
	bool GetResource( const std::wstring& path, const std::wstring& ex, BluePtr<T>& resource, IBlueResManNotifications* notifications = nullptr )
	{
		resource.Unlock();
		return GetResource( path, ex, BlueInterfaceIID<T>(), (void**)&resource, notifications );
	}

	// Returns true if the caller is on the same thread as the main thread queue
	virtual bool IsOnMainThread() = 0;

	// Adds a callback request to any of the queues managed by the resource manager
	virtual bool AddToQueue( BlueResManQueue q, IBlueCallbackMan::CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* id ) = 0;

	// Cancels a previous callback request
	virtual void CancelFromQueue( BlueResManQueue q, uint32_t id ) = 0;

	// Gets the next id for the given queue. Useful to find out if anything was added
	// to a queue (get the id, do something, get the id again and compare).
	virtual uint32_t GetNextIdForQueue( BlueResManQueue q ) = 0;

	// Process the next item from the main thread queue - returns false if the queue was empty
	virtual bool PumpMainThreadQueue() = 0;

	virtual void PauseQueue( BlueResManQueue q ) = 0;
	virtual void ResumeQueue( BlueResManQueue q ) = 0;

	// Loads an object from either a blue file or a red file, based on the extension.
	// 'init' controls whether IInitialize::Initialize is called on loaded objects.
	// This function may yield the calling tasklet while loading the file.
	virtual IRoot* LoadObject( const char* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE ) = 0;
	virtual IRoot* LoadObjectW( const wchar_t* name, Be::LOADOBJECT_INIT_FLAG init = Be::LDOBJ_INITIALIZE ) = 0;
    
    // Saves the given object to either a blue file or a red file, based on the extension.
	virtual bool SaveObject( IRoot* obj, const char* name ) = 0;
    virtual bool SaveObjectW( IRoot* obj, const wchar_t* name ) = 0;

	// Gets a reference to a shared object
	virtual IRoot* GetObject( const std::string& path, const std::string& ex = "" ) = 0;
	virtual IRoot* GetObjectW( const std::wstring& path, const std::wstring& ex = L""  ) = 0;
	
	virtual void ClearCachedObject( const std::string& path, const std::string& ex = "" ) = 0;
	virtual void ClearCachedObjectW( const std::wstring& path, const std::wstring& ex = L""  ) = 0;
	
	virtual void ClearAllCachedObjects() = 0;

	virtual void SetUrgentResourceLoads( bool b ) = 0;
	virtual bool IsUrgentResourceLoads() = 0;

#if BLUE_WITH_PYTHON
	virtual void QueueCallbackForResourceLoads( PyObject* cb, PyObject* args ) = 0;
	virtual void QueueCallbackForUrgentResourceLoads( PyObject* cb, PyObject* args ) = 0;
#endif

	// Notify manager of memory use. This function blocks if reserved memory exceeds
	// limits - other threads must release memory before calling thread is allowed
	// to continue.
	virtual void ReserveBackgroundLoadMemory( size_t size ) = 0;
	virtual void ReleaseBackgroundLoadMemory( size_t size ) = 0;

	virtual unsigned int GetPendingLoads() const = 0;
	virtual unsigned int GetPendingPrepares() const = 0;

	virtual bool GetSubstituteBlackForRed() const = 0;
	virtual void SetSubstituteBlackForRed( bool val ) = 0;

};

extern BLUEIMPORT IBlueResMan* BeResMan;

typedef IBlueResource* (*BlueResManCreateResourceFunction)( const wchar_t* path );

// Helper class for registering file extensions with the resource manager. Used by the
// TRI_REGISTER_RESOURCE_EXTENSION macro below.
class BlueResManRegistrar
{
public:
	BlueResManRegistrar( const wchar_t* ext, BlueResManCreateResourceFunction factory )
	{
		extern void BlueResManRegisterFileExtension( const wchar_t* ext, BlueResManCreateResourceFunction factory );
		BlueResManRegisterFileExtension( ext, factory );
	}
};

#define BLUE_REGISTER_RESOURCE_EXTENSION( ext, factory ) static BlueResManRegistrar CCP_ANONYMOUS_VARIABLE(s_BlueResManRegistrar_)( ext, factory )


#endif
