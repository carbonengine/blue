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


#if USE_RESFILE_2
#include "curl/curl.h"

static CRemoteFileCache s_remoteFileCache;
RemoteFileCache* BeRemoteFileCache = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "remoteFileCache", BeRemoteFileCache );

#endif

static CBlueResMan s_resourceManagerInstance;

IBlueCallbackMan* BeCallbackMan = nullptr;
IBlueResMan* BeResMan = nullptr;
IBlueObjectRecycler* BeRecycler = nullptr;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "resMan", BeResMan );
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "recycler", BeRecycler );

bool BlueInitializeResourceLoading()
{
#ifdef _WIN32
	// Set up a callback managers with threads for each extra hw thread available to us.
	// These are used by the resource manager, texture compression and other
	// background tasks.
	SYSTEM_INFO systemInfo;
	GetSystemInfo( &systemInfo );
	unsigned int threadCount = systemInfo.dwNumberOfProcessors - 1;
	if( threadCount < 1 )
	{
		threadCount = 1;
	}

	if( threadCount > 3 )
	{
		threadCount = 3;
	}
#else
	unsigned int threadCount = 2;
#endif

	if( !BeClasses->CreateInstance( GetBlueCallbackManClsid(), GetIBlueCallbackManIID(), (void**)&BeCallbackMan ) )
	{
		return false;
	}
	BeCallbackMan->SetPriority( 2 );
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
#endif

#if USE_RESFILE_2
	BeRemoteFileCache = &s_remoteFileCache;

	curl_global_init( CURL_GLOBAL_DEFAULT );
#endif

	return true;
}
