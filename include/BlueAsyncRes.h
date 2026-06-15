// Copyright © 2014 CCP ehf.

#pragma once

#ifndef BlueAsyncRes_H
#define BlueAsyncRes_H

#include "IBlueResource.h"
#include "IBluePersist.h"
#include <CcpAtomic.h>

// Silence a warning we get from using std::wstring in the BlueAsyncRes class, which is
// exported from the DLL. We can ignore this warning as uint32_t as all our dlls use the same
// version of the runtime, and use it from a dll.
#ifdef _MSC_VER	
#pragma warning( push )
#pragma warning( disable : 4251 )
#endif

BLUE_DECLARE( BlueAsyncRes );

// Simple interface for receiving notifications from asynchronously loaded resources
class IBlueAsyncResNotifyTarget
{
public:
	virtual void ReleaseCachedData( BlueAsyncRes* p ) = 0;
	virtual void RebuildCachedData( BlueAsyncRes* p ) = 0;
};

BLUE_BLUEIMPORT_CLASS( BlueAsyncRes ) :
	public IBlueResource
{
public:
	EXPOSE_TO_BLUE();

	BlueAsyncRes();
	virtual ~BlueAsyncRes()
	{
		CCP_ASSERT( m_reloadNotifyTargetsCount == 0 );
	}

	// This gets called right before destruction - safe to call virtual functions.
	// It calls the virtual function OnShutdown that subclasses can override if needed.
	void Shutdown();
	
	// Force synchronous load on this particular resource
	void ForceSynchronousLoad();

	// Cancels pending load/prepare. Safe to call on a fully prepared resource.
	void CancelPendingLoad();

	// Add a target for a notification callback that is issued once the resource has
	// finished loading. In the case where loading has finished already the callback
	// is issued right away.
	void AddNotifyTarget( IBlueAsyncResNotifyTarget* p );
	void RemoveNotifyTarget( IBlueAsyncResNotifyTarget* p );

	void Reload();

	// Note that on ORBIS we get a link error in Trinity for typeinfo for BlueAsyncRes if any
	// virtual functions do not have inlined implementations. This is in all likelihood
	// a compiler/linker bug - until it's fixed we get around it by inlining all
	// the virtual functions.

	//////////////////////////////////////////////////////////////////////////
	// IBlueResource
	void Initialize( const wchar_t* name, const wchar_t* ext ) override
	{
		InitializeImpl( name, ext );
	}

	const wchar_t* GetPath() const override
	{
		return m_path.c_str();
	}

	const wchar_t* GetExt() const override
	{
		return m_ext.c_str();
	}

	bool IsLoading() const override
	{
		return m_isLoading == 1;
	}

	bool IsPrepared() const override
	{
		return m_isPrepared == 1;
	}

	bool IsGood() const override
	{
		return m_isGood == 1;
	}

	std::wstring GetFilePath() const;
	std::wstring GetQuery() const;

	typedef std::pair<std::wstring, std::wstring> QueryArgument;
	typedef TrackableStdVector<QueryArgument> QueryArguments;

protected:
	void InitializeImpl( const wchar_t* name, const wchar_t* ext );

	enum LoadingResult
	{
		LR_FAILED, LR_SUCCESS, LR_SUCCESS_DONTNOTIFY
	};

	// Subclasses must provide an implementation for these
	virtual LoadingResult DoLoad() { return LR_FAILED; };
	virtual bool DoPrepare() { return false; };

	// Subclasses can optionally provide implementations for the following
	// function - this is in particular useful for mocking the file operations
	// for testing purposes.
	virtual bool DoOpenStream();

	// Subclasses can optionally provide an implementation for this
	// function - gets called right before final destruction but where
	// it is still safe to call virtual functions.
	virtual void OnShutdown() {}

	// Subclasses can optionally provide an implementation for this function
	// that gets called after DoPrepare or on load failure.
	virtual void CleanupLoadData() {}


protected:

	static void StaticLoadAsync( void* pContext );
	void LoadAsync();
	static void StaticPrepareAsync( void* pContext );
	void PrepareAsync();

	void CloseStream();

	void NotifyRebuildCachedData();
	void NotifyReleaseCachedData();

	// Dummy callback for main thread callback manager, to maintain fence bookkeeping
	static void StaticFailedLoadNotify( void* pContext );


	void SetPrepared( bool b );
	void SetGood( bool b );

	void ParseQuery( const wchar_t* query );

	QueryArgument* FindFirstQueryArgumentByName( const wchar_t* name );
protected:
	std::wstring m_path;
	std::wstring m_ext;
	QueryArguments m_queryArguments;

	IBlueStreamPtr m_dataStream;

	size_t m_reservedMemory;

	// If TRUE, then the resource is being loaded/prepared
	CcpAtomic<uint32_t> m_isLoading;

	// If TRUE, then the resource has finished loading/preparing. Note that subclasses
	// can clear this flag if the object is ever invalidated.
	CcpAtomic<uint32_t> m_isPrepared;

	// If TRUE, then the loading phase succeeded.
	CcpAtomic<uint32_t> m_loadSucceeded;

	// If TRUE, then a notify is required for notify targets.
	CcpAtomic<uint32_t> m_notify;

	// If TRUE, then both load and prepare succeeded and the resource is ready
	// for use.
	CcpAtomic<uint32_t> m_isGood;

	CcpAtomic<uint32_t> m_loadCbId;		/// When set, load has been scheduled on the background loading thread
	CcpAtomic<uint32_t> m_prepareCbId;	    /// When set, D3D mesh preparation has been scheduled on the main thread

	bool m_isUrgent;		/// Keep track of whether urgent resource loads were enabled when this
	/// resource was queued for load - the global setting may have changed
	/// before the prep stage is reached.

	bool m_isForcedSynchronous;

	//typedef TrackableStdList<IBlueAsyncResNotifyTarget*> ReloadNotifyTargetList;
	//ReloadNotifyTargetList m_reloadNotifyTargets;
	IBlueAsyncResNotifyTarget** m_reloadNotifyTargets;
	size_t m_reloadNotifyTargetsCount;
};

#ifdef _MSC_VER	
#pragma warning( pop )
#endif

TYPEDEF_BLUECLASS( BlueAsyncRes );

#endif
