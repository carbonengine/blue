////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		May 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"
#include "ResourceLoading.h"
#include "BlueResMan.h"
#include "CallbackMan.h"
#include "MotherLode.h"
#include "BlueObjectRecycler.h"
#include "Stuffer.h"
#include "RemoteFileCache.h"
#include "BluePaths.h"
#include "BlueThreadMonitor.h"

#include "curl/curl.h"

static CRemoteFileCache s_remoteFileCache;
RemoteFileCache* BeRemoteFileCache = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "remoteFileCache", BeRemoteFileCache );

static CBlueResMan s_resourceManagerInstance;
static CBlueThreadMonitor s_threadMonitorInstance;

IBlueCallbackMan* BeCallbackMan = nullptr;
IBlueResMan* BeResMan = nullptr;
IBlueObjectRecycler* BeRecycler = nullptr;
IBlueThreadMonitor* BeThreadMonitor = nullptr;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "resMan", BeResMan );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "recycler", BeRecycler );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "threadMonitor", BeThreadMonitor );

namespace
{

int GetStartupArgAsInt( const wchar_t* key )
{
	std::wstring value = BeOS->GetStartupArgValue( key );
	int intValue = atoi( CW2A( value.c_str() ) );
	return intValue;
}

unsigned int GetDefaultThreadCount()
{
#ifdef _WIN32
	// Set up a callback managers with threads for each extra hw thread available to us.
	// These are used by the resource manager, texture compression and other
	// background tasks.
	SYSTEM_INFO systemInfo;
	GetSystemInfo( &systemInfo );
	unsigned int threadCount = (systemInfo.dwNumberOfProcessors - 1) * 8;
	if( threadCount < 4 )
	{
		threadCount = 4;
	}

	if( threadCount > 24 )
	{
		threadCount = 24;
	}
#else
	unsigned int threadCount = 8;
#endif

	return threadCount;
}

}


BLUEIMPORT bool BlueInitializeResourceLoading()
{
	BeThreadMonitor = &s_threadMonitorInstance;

	unsigned int threadCount = GetStartupArgAsInt( L"resManThreadCount" );

	if( threadCount == 0 )
	{
		threadCount = GetDefaultThreadCount();
	}

	int priority = 0;
	if( BeOS->HasStartupArg( L"resManThreadPriority" ) )
	{
		priority = GetStartupArgAsInt( L"resManThreadPriority" );
	}

	if( !BeClasses->CreateInstance( GetBlueCallbackManClsid(), GetIBlueCallbackManIID(), (void**)&BeCallbackMan ) )
	{
		return false;
	}

	CCP_LOG( "Starting callback manager with %d threads at priority %d", threadCount, priority );

	BeCallbackMan->SetPriority( priority );
	BeCallbackMan->SetThreadCount( threadCount );
	BeCallbackMan->SetName( "BeCallbackMan" );
	BeCallbackMan->Run();

	//create the MotherLode singleton
	if( !BeClasses->CreateInstance(GetMotherLodeClsid(), GetIMotherLodeIID(), (void**)&BeMotherLode) )
	{
		return false;
	}
	BeMotherLode->Startup();

	BeClasses->CreateInstance( GetBlueObjectRecyclerClsid(), GetBlueObjectRecyclerIID(), (void**)&BeRecycler );

	// Initialize the resource manager
	s_resourceManagerInstance.Initialize();
	BeResMan = &s_resourceManagerInstance;

#if STUFFER_ENABLED
	Stuffer::Startup();
	if( !BeOS->HasStartupArg( L"noStuff" ) )
	{
		BeStuffer->AddFilesFromFolder( BePaths->ResolvePathW( L"app:/" ) );
	}
#endif

	BeRemoteFileCache = &s_remoteFileCache;

	return true;
}
