// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "BlueResMan.h"
#include "BlueResManBackgroundCall.h"
#include "CallbackMan.h"
#include "YamlWriter.h"
#include "YamlReader.h"
#include "BlueMemStream.h"
#include "BlackWriter.h"
#include "BlackReader.h"
#include "BlueResFile.h"
#include "BlueAsyncRes.h"
#include <BlueStatistics.h>
#include "IBlueResource.h"
#include "IBluePersist.h"
#include "IBlueOS.h"
#include "IMotherLode.h"
#include "BlueFileUtil.h"
#include "IBlueObjectBuilder.h"
#include "IBluePaths.h"
#include <Scheduler.h>

#include <cctype>
#include <cwctype>
#include <thread>

BLUE_DEFINE( BlueResMan );

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "ResMan" );

CCP_STATS_DECLARE( resManCancelCount,			"Blue/resMan/CancelCount",			true,	CST_COUNTER_LOW, "Count of calls per frame to the resource manager CancelFromQueue function" );
CCP_STATS_DECLARE( resManPendingLoads,			"Blue/resMan/PendingLoads",			false,	CST_COUNTER_LOW, "Pending loads in the resource manager" );
CCP_STATS_DECLARE( resManPendingPrepares,		"Blue/resMan/PendingPrepares",		false,	CST_COUNTER_LOW, "Pending prepares in the resource manager" );
CCP_STATS_DECLARE( resManLoadObjectCalls,		"Blue/resMan/LoadObjectCalls",		false,	CST_COUNTER_LOW, "Number of calls to LoadObject" );
CCP_STATS_DECLARE( resManLoadObjectCacheHit,	"Blue/resMan/LoadObjectCacheHit",	false,	CST_COUNTER_LOW, "Number of cache hits in LoadObject" );
CCP_STATS_DECLARE( resManLoadObjectShared,		"Blue/resMan/LoadObjectShared",		false,	CST_COUNTER_LOW, "Number of times LoadObject results in a shared builder" );
CCP_STATS_DECLARE( resManGetResourceCalls,		"Blue/resMan/GetResourceCalls",		false,	CST_COUNTER_LOW, "Number of calls to GetResource" );
CCP_STATS_DECLARE( resManGetResourceCacheHit,	"Blue/resMan/GetResourceCacheHit",	false,	CST_COUNTER_LOW, "Number of cache hits in GetResource" );
CCP_STATS_DECLARE( resManGetResourceShared,		"Blue/resMan/GetResourceShared",	false,	CST_COUNTER_LOW, "Number of times GetResource results in a shared resource" );
CCP_STATS_DECLARE( resManLoadObject,			"Blue/resMan/LoadObject",			false,	CST_TIME,		 "Accumulated time in LoadObject" );

typedef TrackableStdMap<std::wstring, BlueResMan::CreateResourceFunction> TypeMap;

namespace
{
	// The type map stores factory functions registered under file extensions.
	// It is accessed via this function to prevent issues with order-of-initialization
	// when registering file types.
	TypeMap& GetTypeMap()
	{
		static TypeMap s_types( "BlueResMan/s_types" );

		return s_types;
	}
}

void BLUEIMPORT BlueResManRegisterFileExtension( const wchar_t* ext, BlueResManCreateResourceFunction factory )
{
	std::wstring extension = ext;
	for( std::wstring::iterator it = extension.begin(); it != extension.end(); ++it )
	{
		*it = std::tolower( *it );
	}
	GetTypeMap()[extension] = factory;
}

BlueResMan::BlueResMan( IRoot* lockobj ) :
	PARENTLOCK( m_loadObjectCache ),
	m_loadObjectCacheEnabled( true ),
	m_loadObjectTimeSlice( 0.005f ),
	m_urgentResourceLoads( false ),
	m_mainThread( 0 ),
	m_backgroundLoadMemoryBudget( 256*1024*1024 ),
	m_backgroundLoadMemoryInUse( 0 ),
	m_mainThreadTimeSlice( 0.005f ),
	m_mainThreadMaxTime( 0.0f ),
	m_pendingLoads( 0 ),
	m_pendingPrepares( 0 ),
	m_preparesHandledLastTick( 0 ),
	m_preparesHandledPerTickMax( 0 ),
	m_preparesHandledTotal( 0 ),
	m_maxAllowedInPrepareQueue( 256 ),
	m_loadQueueTimeAverage( 0.0f ),
	m_loadQueueTimeMax( 0.0f ),
	m_prepareQueueTimeAverage( 0.0f ),
	m_prepareQueueTimeMax( 0.0 ),
	m_backgroundLoadMemoryMutex( "BlueResMan", "m_backgroundLoadMemoryMutex" )
{
	m_loadObjectCache.SetCacheSize( 1*1024*1024 );
}

BlueResMan::~BlueResMan()
{
}

static const char* s_tickCookie = "BeResMan";

void BlueResMan::Initialize()
{
	CCP_ASSERT( m_threadQueues[BRMQ_MAIN] == NULL );
	CCP_ASSERT( m_threadQueues[BRMQ_BACKGROUND] == NULL );

	// Create an instance to manage tasks to be run on the main thread. This instance does
	// not create its own thread, but is polled in the main loop.
	BeClasses->CreateInstance( GetBlueCallbackManClsid(), GetIBlueCallbackManIID(), (void**)&m_threadQueues[BRMQ_MAIN] );

	m_threadQueues[BRMQ_BACKGROUND] = BeCallbackMan;

	m_mainThread = CcpGetCurrentThreadId();

	BeOS->RegisterForTicks( this, (void*)s_tickCookie );

	m_loadObjectCache.Startup();
}

void BlueResMan::Shutdown()
{
	m_loadObjectCache.Shutdown();
	if( BeOS )
	{
		BeOS->UnregisterForTicks(this, (void*)s_tickCookie );
	}

	m_threadQueues[BRMQ_BACKGROUND].Unlock();
	m_threadQueues[BRMQ_MAIN].Unlock();
}

bool BlueResMan::GetResource( const std::string& path, const std::string& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CA2W p(path.c_str());
	CA2W e(ex.c_str());

	std::wstring pathW( p );
	std::wstring exW( e );

	return GetResourceW( pathW, exW, iid, resource, notifications );
}

bool BlueResMan::GetResourceW( const std::wstring& path, const std::wstring& ex, const Be::IID& iid, void** resource, IBlueResManNotifications* notifications )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT( resource );
	
	*resource = nullptr;
	
	IBlueResource* tmp = GetResourceHelper( path, ex, notifications );
	if( !tmp )
	{
		return false;
	}

	bool ok = tmp->QueryInterface( iid, resource, BEQI_SILENT );

	// The QueryInterface call added a reference - release the one we
	// got from the GetResource call.
	tmp->Unlock();

	return ok;
}

IBlueResource* BlueResMan::GetResourceHelper( const std::wstring& path, const std::wstring& ex, IBlueResManNotifications* notifications )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( path.empty() )
	{ 
		return NULL;
	}

	CCP_STATS_INC( resManGetResourceCalls );

	IBlueResource* result = nullptr;

    std::wstring key;
	NormalizeResPath( path.c_str(), key );
    key += ex;

	switch( BeMotherLode->Lookup( key.c_str(), GetIBlueResourceIID(), (void**)&result ) )
	{
		case IMotherLode::LOOKUP_FAILED:
			return nullptr;

		case IMotherLode::LOOKUP_CACHED:
			if( result )
			{
				CCP_STATS_INC( resManGetResourceCacheHit );
				if( notifications )
				{
					notifications->OnResourceFromCache( result );
				}

				// The Lookup call will have added a reference
				return result;
			}
		
		case IMotherLode::LOOKUP_SUCCESS:
			if( result )
			{
				CCP_STATS_INC( resManGetResourceShared );
				if( notifications )
				{
					notifications->OnResourceFromCache( result );
				}

				// The Lookup call will have added a reference
				return result;
			}
	}

	// Not found - create new instance

	std::wstring protocol;
	GetResProtocol( key.c_str(), protocol );
	if( protocol == L"dynamic" )
	{
		const wchar_t* slash = wcschr( key.c_str() + 9, L'/' );

		std::wstring name = slash ? std::wstring( key.c_str() + 9, slash - key.c_str() - 9 ) : std::wstring( key.c_str() + 9 );
		DynamicConstructors::iterator constructor = m_dynamicConstructors.find( name );
		if( constructor == m_dynamicConstructors.end() )
		{
			CCP_LOGERR_CH( s_ch, "No dynamic constructor found for resource name, %S", key.c_str() );
			return NULL;
		}

		result = constructor->second->GetResource( slash ? slash + 1 : L"" );
		if( !result )
		{
			return NULL;
		}

		if( notifications )
		{
			notifications->OnResourceCreated( result );
		}

		bool isOK = BeMotherLode->Insert( key.c_str(), result, true, 0, IMotherLode::CACHING_NOT_ALLOWED );
		CCP_ASSERT( isOK );
        CCP_UNUSED( isOK );
	}
	else
	{
		auto query = path.find( '?' );
		// Get the extension from the path
		size_t lastDotIx = path.find_last_of( '.', query );
		std::wstring extension = path.substr( lastDotIx + 1, query - lastDotIx - 1 );
		if( extension.empty() )
		{
			// No extension found
			CCP_LOGERR_CH( s_ch, "BlueResMan::GetResourceW: Must have an extension to map to a factory (%S, %S)", path.c_str(), ex.c_str() );
			return result;
		}

		extension += ex;

		for( std::wstring::iterator it = extension.begin(); it != extension.end(); ++it )
		{
			*it = std::tolower( *it );
		}

		// Find a factory based on the file extension
		TypeMap::iterator typeIt = GetTypeMap().find( extension );
		if( typeIt == GetTypeMap().end() )
		{
			// No factory found for this extension
			CCP_LOGERR_CH( s_ch, "BlueResMan::GetResourceW: No factory found for extension (%S, %S)", path.c_str(), ex.c_str() );
			return result;
		}

		// Create the resource by calling the factory.
		result = typeIt->second( path.c_str() );

		IWeakObjectPtr wrp = BlueCastPtr( result );
		CCP_ASSERT_M( wrp, "Resource class types must support weak references" );

		if( notifications )
		{
			notifications->OnResourceCreated( result );
		}

		result->Initialize( path.c_str(), ex.c_str() );

		bool isOK = BeMotherLode->Insert( key.c_str(), result );
		CCP_ASSERT( isOK );
        CCP_UNUSED( isOK );
	}

	return result;
}

void BlueResMan::RegisterResourceConstructor( const wchar_t* name, IBlueDynamicResourceConstructor* constructor )
{
	std::wstring key = name;
	for( std::wstring::iterator it = key.begin(); it != key.end(); ++it )
	{
		*it = std::towlower( *it );
	}

	m_dynamicConstructors[key].reset( constructor );
}

void BlueResMan::RegisterScriptResourceConstructor( const wchar_t* name, BlueScriptCallback constructor )
{
	RegisterResourceConstructor( name, new BluePythonDynamicResourceConstructor( constructor ) );
}

void BlueResMan::UnregisterResourceConstructor( const wchar_t* name )
{
	std::wstring key = name;
	for( std::wstring::iterator it = key.begin(); it != key.end(); ++it )
	{
		*it = std::towlower( *it );
	}

	DynamicConstructors::iterator it = m_dynamicConstructors.find( key );
	if( it != m_dynamicConstructors.end() )
	{
		m_dynamicConstructors.erase( it );
	}
}

bool BlueResMan::IsOnMainThread()
{
	return CcpGetCurrentThreadId() == m_mainThread;
}

bool BlueResMan::AddToQueue( BlueResManQueue q, IBlueCallbackMan::CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* pId )
{
	CCP_ASSERT( q < BRMQ_COUNT );
	
	if( m_threadQueues[q] )
	{
		return m_threadQueues[q]->Add( pCb, pContext, flags, pId );
	}
	else
	{
		return 0;
	}
}

void BlueResMan::CancelFromQueue( BlueResManQueue q, uint32_t id )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT( q < BRMQ_COUNT );

	CCP_STATS_INC( resManCancelCount );

	if( m_threadQueues[q] )
	{
		m_threadQueues[q]->Cancel( id );
	}
}

uint32_t BlueResMan::GetNextIdForQueue( BlueResManQueue q )
{
	CCP_ASSERT( q < BRMQ_COUNT );

	if( m_threadQueues[q] )
	{
		return m_threadQueues[q]->GetNextId();
	}
	else
	{
		return 0;
	}
}

bool BlueResMan::PumpMainThreadQueue()
{
	CCP_ASSERT( m_threadQueues[BRMQ_MAIN] );

	return m_threadQueues[BRMQ_MAIN]->Update();
}

void BlueResMan::PauseQueue( BlueResManQueue q )
{
	CCP_ASSERT( q < BRMQ_COUNT );

	if( m_threadQueues[q] )
	{
		return m_threadQueues[q]->Pause();
	}
}

void BlueResMan::ResumeQueue( BlueResManQueue q )
{
	CCP_ASSERT( q < BRMQ_COUNT );

	if( m_threadQueues[q] )
	{
		return m_threadQueues[q]->Resume();
	}
}

void BlueResMan::ReserveBackgroundLoadMemory( size_t size )
{
	for( int numTries = 0; numTries < 50; ++numTries )
	{
		{
			CcpAutoMutex guard( m_backgroundLoadMemoryMutex );
			if( m_backgroundLoadMemoryInUse + size < m_backgroundLoadMemoryBudget )
			{
				m_backgroundLoadMemoryInUse += size;
				return;
			}

		}
		CcpThreadSleep( 20 );
	}

	CCP_LOGWARN_CH( s_ch, "BlueResMan::ReserveBackgroundLoadMemory timed out - allowing budget overrun to prevent potential deadlock" );
}

void BlueResMan::ReleaseBackgroundLoadMemory( size_t size )
{
	CcpAutoMutex guard( m_backgroundLoadMemoryMutex );
	if( m_backgroundLoadMemoryInUse < size )
	{
		// Our bookkeeping must be off!
		m_backgroundLoadMemoryInUse = 0;
	}
	else
	{
		m_backgroundLoadMemoryInUse -= size;
	}
}

void BlueResMan::OnTick( Be::Time realTime, Be::Time simTime, void* cookie )
{
	Update();
}

void BlueResMan::Update()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT( m_threadQueues[BRMQ_MAIN] );
	CCP_ASSERT( m_threadQueues[BRMQ_BACKGROUND] );

	BeTimer t;
	float timeInUpdate = 0.0f;
	m_preparesHandledLastTick = 0;
	unsigned int prepareQueueSize = 0;
	do
	{
		m_threadQueues[BRMQ_BACKGROUND]->Unthrottle();
		bool didSomething = m_threadQueues[BRMQ_MAIN]->Update();
		timeInUpdate = (float)t.GetSeconds();
		if( !didSomething )
		{
			// Nothing was processed - no need to continue looping
			break;
		}
		++m_preparesHandledLastTick;
		prepareQueueSize = m_threadQueues[BRMQ_MAIN]->GetSize();
	}
	while( (timeInUpdate < m_mainThreadTimeSlice) || (prepareQueueSize > m_maxAllowedInPrepareQueue) );

	m_preparesHandledTotal += m_preparesHandledLastTick;
	if( m_preparesHandledLastTick > m_preparesHandledPerTickMax )
	{
		m_preparesHandledPerTickMax = m_preparesHandledLastTick;
	}

	if( timeInUpdate > m_mainThreadMaxTime )
	{
		m_mainThreadMaxTime = timeInUpdate;
	}

	m_pendingPrepares = m_threadQueues[BRMQ_MAIN]->GetSize();
	m_pendingLoads = m_threadQueues[BRMQ_BACKGROUND]->GetSize();

	m_prepareQueueTimeMax = m_threadQueues[BRMQ_MAIN]->GetTimeInQueueMax();
	m_prepareQueueTimeAverage = m_threadQueues[BRMQ_MAIN]->GetTimeInQueueAverage();

	m_loadQueueTimeMax = m_threadQueues[BRMQ_BACKGROUND]->GetTimeInQueueMax();
	m_loadQueueTimeAverage = m_threadQueues[BRMQ_BACKGROUND]->GetTimeInQueueAverage();

	CCP_STATS_SET( resManPendingLoads, m_pendingLoads );
	CCP_STATS_SET( resManPendingPrepares, m_pendingPrepares );
}

void BlueResMan::SetUrgentResourceLoads( bool b )
{
	m_urgentResourceLoads = b;
}

bool BlueResMan::IsUrgentResourceLoads()
{
	return m_urgentResourceLoads;
}

void BlueResMan::ResetQueueStats()
{
	m_threadQueues[BRMQ_BACKGROUND]->ResetQueueStats();
	m_threadQueues[BRMQ_MAIN]->ResetQueueStats();
}

unsigned int BlueResMan::GetPendingLoads() const
{
	return m_threadQueues[BRMQ_BACKGROUND]->GetSize();
}

unsigned int BlueResMan::GetPendingPrepares() const
{
	return m_threadQueues[BRMQ_MAIN]->GetSize();
}

void BlueResMan::SetLoadingThreadPriority( int prio )
{
	m_threadQueues[BRMQ_BACKGROUND]->SetPriority( prio );
}

void BlueResMan::SetLoadingThreadCount( int n )
{
	if( n < 1 )
	{
		n = 1;
	}
	m_threadQueues[BRMQ_BACKGROUND]->Stop();
	m_threadQueues[BRMQ_BACKGROUND]->SetThreadCount( n );
	m_threadQueues[BRMQ_BACKGROUND]->Run();
}

#if BLUE_WITH_PYTHON
static PyObject* PySetUrgentResourceLoads( PyObject* self, PyObject* args )
{
	bool isUrgent;
	if( !PyArg_ParseTuple( args, "b", &isUrgent ) )
	{
		return NULL;
	}

	BeResMan->SetUrgentResourceLoads(isUrgent);

	Py_INCREF(Py_None);
	return Py_None;
}
#endif

IRootPtr BlueResMan::LoadObject(const char *name, Be::LOADOBJECT_INIT_FLAG init /* = LDOBJ_INITIALIZE */)
{
	CA2W wn(name);
	return LoadObject(wn, init);
}

class RecursionLimiter
{
public:
	RecursionLimiter(const wchar_t* msg, int maxDepth) :
		m_name(msg),
		m_maxDepth(maxDepth),
		m_depth(0)
	{
	}

	void Enter(const wchar_t* name)
	{
		++m_depth;
		if(m_depth >= m_maxDepth)
		{
			CCP_LOGERR_CH(s_ch, "%S: maximum recursion limit reached for '%S'", m_name, name);
		}
	}

	void Leave()
	{
		CCP_ASSERT(m_depth > 0);
		--m_depth;
	}

	bool IsOK()
	{
		return m_depth < m_maxDepth;
	}

	void Reset()
	{
		m_depth = 0;
	}

	unsigned int GetDepth()
	{
		return m_depth;
	}

private:
	const wchar_t* m_name;
	unsigned int m_maxDepth;
	unsigned int m_depth;
};

#if BLUE_WITH_PYTHON

// A key to identify the calling tasklet+thread.
// Required because LoadObject can be called from multiple threads
// without any Python state (m_tasklet can be nullptr).
struct RecursionLimiterKey
{
	RecursionLimiterKey(std::thread::id threadId, PyObject* tasklet) : m_threadId(threadId), m_tasklet(tasklet){};
	friend bool operator <(const RecursionLimiterKey& lhs, const RecursionLimiterKey& rhs)
	{
		if(lhs.m_threadId < rhs.m_threadId)
		{
			return true;
		}
		if(lhs.m_threadId == rhs.m_threadId)
		{
			return lhs.m_tasklet < rhs.m_tasklet;
		}
		return false;
	}
private:
	std::thread::id m_threadId;
	PyObject* m_tasklet;
};

typedef TrackableStdMap<RecursionLimiterKey, RecursionLimiter> RecursionLimiterMap_t;
static RecursionLimiterMap_t s_limiterPerTasklet("BlueResMan/s_limiterPerTasklet");

class RecursionLimiterHelper
{
public:
	RecursionLimiterHelper(RecursionLimiter& rl, RecursionLimiterKey key)
		: m_recursionLimiter(rl),
		m_key(key)
	{
	}

	~RecursionLimiterHelper()
	{
		m_recursionLimiter.Leave();
		if(m_recursionLimiter.GetDepth() == 0)
		{
			s_limiterPerTasklet.erase(m_key);
		}
	}

private:
	RecursionLimiter& m_recursionLimiter;
	RecursionLimiterKey m_key;

};
#endif


IRootPtr BlueResMan::LoadObject(const wchar_t* unnormalizedName, Be::LOADOBJECT_INIT_FLAG init /* = LDOBJ_INITIALIZE */)
{
	CCP_STATS_SCOPED_TIME(resManLoadObject);
	CCP_STATS_INC(resManLoadObjectCalls);

	std::wstring nameString;
	NormalizeResPath(unnormalizedName, nameString);

	if(nameString.empty())
	{
		CCP_LOGERR_CH(s_ch, "LoadObject called with an empty path");
		return nullptr;
	}

	const wchar_t* name = nameString.c_str();

#if CCP_STACKLESS

	// Get a recursion limiter for the current tasklet. Recursion depth has to be tracked
	// per tasklet, otherwise the preloading of files we do below, which yields to other
	// tasklets that my in turn start loading objects can look like very deep recursion.
	// See: PyOS->Yield()
	PyObject* myTasklet{nullptr};
	if( PyGILState_Check() )
	{
		myTasklet = SchedulerAPI()->PyScheduler_GetCurrent();
		// Release the reference right away. We use it as a key for the map, but we don't
		// need to hold a strong reference. Using it as a key without affecting the reference
		// count ought to be safe - the tasklet object won't die while we're running on it.
		Py_XDECREF( myTasklet );
	}

	RecursionLimiter* limit = NULL;
	RecursionLimiterKey limiterKey(std::this_thread::get_id(), myTasklet);

	{
		// The insert function returns an iterator pointing to the value matching the key (myTasklet).
		// This can be an existing value, or a newly inserted one if they wasn't found in the map.
		// Note that in debug builds the container keeps track of its iterators. This tracking
		// is invalidated if we add items to the container on another tasklet before the iterator
		// is destroyed. This is why we have the pair in its own scope, and requiring us to use a
		// pointer to the limiter rather than a reference as we can't assign to it after declaring
		// the variable.
		std::pair<RecursionLimiterMap_t::iterator, bool> limitInsert;
		limitInsert = s_limiterPerTasklet.insert( RecursionLimiterMap_t::value_type( limiterKey, RecursionLimiter( L"BlueOS::LoadObjectW", 25 ) ) );
		limit = &(limitInsert.first->second);
	}

	limit->Enter( name );

	// Make sure we call Leave - so many returns from this function
	RecursionLimiterHelper onExit( *limit, limiterKey );

	if( !limit->IsOK() )
	{
		return NULL;
	}

#endif

	if( !name )
	{
		BeOS->SetError( BEDEF, 0, "%s: missing filename", __FUNCTION__ );
		return NULL;
	}

	std::wstring filename = name;

	IBlueObjectBuilderPtr builder;
	if( m_loadObjectCacheEnabled )
	{
		switch( m_loadObjectCache.Lookup( filename.c_str(), GetIBlueObjectBuilderIID(), (void**)&builder ) )
		{
			case IMotherLode::LOOKUP_FAILED:
				// The cache lookup itself failed - something must be corrupt
				CCP_LOGERR_CH( s_ch, "%s: Lookup in loadObjectCache failed", __FUNCTION__ );
				return nullptr;

			case IMotherLode::LOOKUP_SUCCESS:
				if( builder )
				{
					CCP_STATS_INC( resManLoadObjectShared );
				}
				break;

			case IMotherLode::LOOKUP_CACHED:
				if( builder )
				{
					CCP_STATS_INC( resManLoadObjectCacheHit );
				}
				break;
		}
	}

	if( !builder )
	{
		// Builder was not found in cache - create it. First we create the file object
		// and read the file.
		IBlueStreamPtr sourceStream;

		auto result = GetFileContentsWithYield(filename, sourceStream);
		if( !BeIsSuccess( result ) )
		{
			CCP_LOGERR_CH( s_ch, "%s", result.value.c_str() );
			return nullptr;
		}

		IRootReaderPtr reader;
		GetReaderForStream( filename, sourceStream, reader );

		builder = BlueCastPtr( reader );

		if( !builder )
		{
			return nullptr;
		}

		if( m_loadObjectCacheEnabled )
		{
			m_loadObjectCache.Insert( filename.c_str(), builder );
		}
	}

	IRootReaderPtr rd = BlueCastPtr( builder );
	if( rd )
	{
		rd->SetDoInitialize( init == Be::LDOBJ_INITIALIZE );
		rd->SetTimeSlice( m_loadObjectTimeSlice );
	}

	CCP_LOG_CH( s_ch, "Creating object from %S", filename.c_str() );
	IRootPtr obj;
	obj.Attach( builder->CreateObjectWithYield( 0, nullptr ) );

	if( !obj )
	{
		std::string msg;
		builder->GetErrorMessage( msg );
		CCP_LOGERR_CH( s_ch, msg.c_str() );
	}
	return obj;
}


void BlueResMan::GetReaderForStream( std::wstring filename, IBlueStream* sourceStream, IRootReaderPtr& reader )
{
	reader = nullptr;

	BlackReaderPtr blackReader;
	blackReader.CreateInstance();

	CCP_LOG_CH( s_ch, "Reading %S", filename.c_str() );
	blackReader->SetFileName( filename.c_str() );

	CCP_ASSERT( sourceStream );
	if( blackReader->IsHeaderValid( sourceStream ) )
	{
		sourceStream->Seek( 0, ICcpStream::SO_BEGIN );
		if( blackReader->ReadForCachingFromStream( sourceStream ) )
		{
			reader = blackReader;
		}
		else
		{
			std::string msg;
			blackReader->GetErrorMessage( msg );
			CCP_LOGERR_CH( s_ch, "Error reading %S: %s", filename.c_str(), msg.c_str() );
		}
	}
	else
	{
		sourceStream->Seek( 0, ICcpStream::SO_BEGIN );

		YamlReaderPtr yamlReader;
		yamlReader.CreateInstance();
		yamlReader->SetFileName( filename.c_str() );

		if( yamlReader->ReadForCachingFromStream( sourceStream ) )
		{
			reader = yamlReader;
		}
		else
		{
			std::string msg;
			yamlReader->GetErrorMessage( msg );
			CCP_LOGERR_CH( s_ch, "Error reading %S: %s", filename.c_str(), msg.c_str() );
		}
	}
}

#if BLUE_WITH_PYTHON
PyObject* BlueResMan::PyLoadObjectFromYamlString( PyObject* self, PyObject* args )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PyObject* pyCharStream;

	if( !PyArg_ParseTuple( args, "O", &pyCharStream ) )
	{
		return NULL;
	}

	if (!PyUnicode_Check(pyCharStream))
	{
		PyErr_SetString(PyExc_TypeError, "LoadObjectFromYamlString expected a non-unicode string.");
		return NULL;
	}

	IBlueStreamPtr newStream;
	newStream.CreateInstance(GetMemStreamClsid());
	if (newStream)
	{
		const char* charString = PyUnicode_AsUTF8(pyCharStream);
		ssize_t len = strlen(charString);

		MemStream *memStream = static_cast<MemStream*>( static_cast<IBlueStream*>( newStream ) );
		memStream->Write(charString, len);
		memStream->Seek(0, ICcpStream::SO_BEGIN);

		CYamlReader w;
		IRoot* obj = w.ReadFromStream( memStream );
		if( obj )
		{
			PyObject* pyObj = BlueWrapObjectForPython( obj );
			obj->Unlock();
			return pyObj;
		}
	}
	Py_RETURN_NONE;
}
#endif

bool BlueResMan::SaveObject( IRoot* obj, const char* name )
{
	CA2W wn(name);
	return SaveObjectW(obj, wn);
}

bool BlueResMan::SaveObjectW( IRoot* obj, const wchar_t* name )
{
	if( !name )
	{
		BeOS->SetError( BEDEF, 0, "%s: missing filename", __FUNCTION__ );
		return false;
	}

	const wchar_t *dot = wcsrchr(name, L'.');
	if (!dot)
	{
		BeOS->SetError( BEDEF, 0, "%s: invalid filename, missing extension (\"%ls\")", __FUNCTION__, name );
		return false;
	}

	IRootWriter* w;
		
	if( wcscmp( dot, L".black" ) == 0 )
	{
		w = CCP_NEW( "BlueResMan::SaveObjectW BlackWriter" ) CBlackWriter;
	}
	else
	{
		w = CCP_NEW( "BlueResMan::SaveObjectW YamlWriter" ) CYamlWriter;
	}
	auto result = w->WriteObjectToFile( obj, name );

	CCP_DELETE w;
	return BeIsSuccess( result );
}

Be::Result<std::string> BlueResMan::GetResourceFromScript( const std::wstring& path, const std::wstring& ex, IRoot** resource )
{
	IBlueResourcePtr res;
	if( GetResourceW( path.c_str(), ex.c_str(), GetIBlueResourceIID(), (void**)&res ) )
	{
		*resource = res.Detach();
		return Be::Result<std::string>();
	}

	*resource = nullptr;
	return Be::Result<std::string>("GetResource call failed");
}

Be::Result<std::string> BlueResMan::Wait()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	unsigned int pendingLoads = GetPendingLoads();
	unsigned int pendingPrepares = GetPendingPrepares();

	if( pendingLoads + pendingPrepares == 0 )
	{
		// Nothing to wait for
		return Be::Result<std::string>();
	}

#if CCP_STACKLESS
	if( !PyOS->CanYield() )
	{
		//this is a tasklet that cannot block
		return Be::Result<std::string>( "This tasklet cannot block" );
	}

	CCP_LOG( "BlueResMain waiting for loads - %d pending loads, %d pending prepares", pendingLoads, pendingPrepares );
	Be::Time before = BeOS->GetActualTime();

	// Waiting is done by queuing a request on the background queue. Once processed, it
	// queues a request on the main thread queue. For waiting, we don't need an actual
	// callback, just a marker that flows through the queues (with a fence).
	BlueResManBackgroundCall::Issue( nullptr, IBlueCallbackMan::BCBF_FENCE );

	
	Be::Time now = BeOS->GetActualTime();
	Be::Time delta = now - before;
	float secs = TimeAsFloat( delta );
	CCP_LOG( "BlueResMain waited for %g seconds", secs );

#else

	while( pendingLoads + pendingPrepares > 0 )
	{
		Update();

		pendingLoads = GetPendingLoads();
		pendingPrepares = GetPendingPrepares();
	}

#endif

	return Be::Result<std::string>();
}

Be::Result<std::string> BlueResMan::WaitUrgent()
{
#if CCP_STACKLESS
	if( !PyOS->CanYield() )
	{
		//this is a tasklet that cannot block
		return Be::Result<std::string>( "This tasklet cannot block" );
	}

	BlueResManBackgroundCall::Issue( nullptr, IBlueCallbackMan::BCBF_FENCE | IBlueCallbackMan::BCBF_URGENT );

	return Be::Result<std::string>();
#else

	return Wait();

#endif
}

Be::Result<std::string> BlueResMan::LoadObjectFromScript( const std::wstring& path, IRoot** obj )
{
	*obj = LoadObject( path.c_str(), Be::LDOBJ_INITIALIZE ).Detach();
	return Be::Result<std::string>();
}

Be::Result<std::string> BlueResMan::LoadObjectWithoutInitializeFromScript( const std::wstring& path, IRoot** obj )
{
	*obj = LoadObject( path.c_str(), Be::LDOBJ_DONT_INITIALIZE ).Detach();
	return Be::Result<std::string>();
}

Be::Result<std::string> BlueResMan::GetFileContentsWithYield( std::wstring filename, IBlueStreamPtr& sourceStream )
{
	if( BePaths->FileExistsLocally( filename.c_str() ) )
	{
		return BePaths->GetFileContentsWithYield( filename, &sourceStream );
	}

#if CCP_STACKLESS
	if( PyOS->CanYield() )
	{
		// This is to prevent multiple tasklets from triggering download
		// of the same file. This mainly happens in the Probe, but could
		// happen in the client as well, for example the first time
		// a missile type is seen.
		auto foundIt = m_filesInProgress.find( filename );
		while( foundIt != m_filesInProgress.end() )
		{

			PyOS->Yield();
			foundIt = m_filesInProgress.find( filename );
		}
	}
#endif
    
	m_filesInProgress.insert( filename );
	auto result = BePaths->GetFileContentsWithYield( filename, &sourceStream );
	m_filesInProgress.erase( filename );

	return result;
}


BluePythonDynamicResourceConstructor::BluePythonDynamicResourceConstructor( BlueScriptCallback callable )
	:m_callable( callable )
{
}

IBlueResource* BluePythonDynamicResourceConstructor::GetResource( const wchar_t* query )
{
	if( !m_callable )
	{
		return nullptr;
	}

	IBlueResource* result = nullptr;
	if( !m_callable.Call( result, query ) )
	{
		CCP_LOGERR_CH( s_ch, "Error while calling script dynamic resource constructor" );
		PyErr_Clear();
		return nullptr;
	}

	if( !result )
	{
		CCP_LOGERR_CH( s_ch, "Python dynamic resource constructor returned incorrect value (IBlueResource is expected)" );
	}

	return result;
}

IBlueResMan* GetBeResMan()
{
	return BeResMan;
}
