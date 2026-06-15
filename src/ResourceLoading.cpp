// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "ResourceLoading.h"
#include "BlueResMan.h"
#include "CallbackMan.h"
#include "MotherLode.h"
#include "BlueObjectRecycler.h"
#include "RemoteFileCache.h"
#include "BluePaths.h"
#include "BlueThreadMonitor.h"
#include "BlueObjectMetadata.h"
#include "BlueSysInfo.h"

#include "curl/curl.h"

static CRemoteFileCache s_remoteFileCache;
RemoteFileCache* BeRemoteFileCache = &s_remoteFileCache;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "remoteFileCache", BeRemoteFileCache );

static CBlueResMan s_resourceManagerInstance;
static CBlueThreadMonitor s_threadMonitorInstance;

IBlueCallbackMan* BeCallbackMan = nullptr;
IBlueResMan* BeResMan = &s_resourceManagerInstance;
static CBlueObjectRecycler s_blueObjectRecyclerInstance;
IBlueObjectRecycler* BeRecycler = &s_blueObjectRecyclerInstance;
IBlueThreadMonitor* BeThreadMonitor = &s_threadMonitorInstance;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "resMan", BeResMan );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "recycler", BeRecycler );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "threadMonitor", BeThreadMonitor );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "objectMetadata", BeObjectMetadata );

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
	// Set up a callback managers with threads for each extra hw thread available to us.
	// These are used by the resource manager, texture compression and other
	// background tasks.
	unsigned int threadCount = ( BlueSysInfo::GetSysInfo().GetCpuInfo().m_logicalCpuCount - 1 ) * 8;
	if( threadCount < 4 )
	{
		threadCount = 4;
	}

	if( threadCount > 24 )
	{
		threadCount = 24;
	}
	return threadCount;
}

}


BLUEIMPORT bool BlueInitializeResourceLoading()
{
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

	BeMotherLode->Startup();

	// Initialize the resource manager
	s_resourceManagerInstance.Initialize();

	return true;
}
