// Copyright © 2008 CCP ehf.

/* 
	*************************************************************************

	MotherLode.cpp

	Description:

		A new general instance manager for blue objects.  Stores objects
		that support the IWeakRef interface, so that one can access them
		via a unicode key while they are alive.
		Additionally, if they support the ICaceable interface, they will
		Be stored for an additional amount of time (cached) if depending
		on the amount of memory that they consume.

	Dependencies:

		Blue

	*************************************************************************
*/
#include "StdAfx.h"

#include "MotherLode.h"
#if BLUE_WITH_PYTHON
#include "BluePythonWeakRef.h"
#endif
#include <BlueStatistics.h>

static CMotherLode s_beMotherLode;
IMotherLode* BeMotherLode = &s_beMotherLode;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "motherLode", BeMotherLode );

CcpLogChannel_t s_ml = CCP_LOG_DEFINE_CHANNEL( "MotherLode" );

static char cookie[] = "Motherlode";

MotherLode::MotherLode( IRoot* lockobj ) :
	mMap("MotherLode"),
	mLRU("MotherLode"),
	mPending("MotherLode"),
	mMemUsage(0),
	mMaxMemUsage(32*1024*1024),
	mActive(false),
	mVerbose(false)
{}


MotherLode::~MotherLode()
{
	Clear();
}


bool MotherLode::Insert(
	const wchar_t*	key,				//the key to assign this to
	IRoot*			object,				//the object to store
	bool			replace,			//if true, replace any object found with the same key 
	bool			*inserted,			//Was the object inserted?
	ResourceCaching	allowCaching		//allow caching of this resource
	)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT(key);
	CCP_ASSERT(object);
	
	if (inserted)
		*inserted = false;
	if (!mActive) {
		//this system isn't up and running.
		return true;
	}

	//Get the WeakObject pointer
	IWeakObjectPtr wrp = BlueCastPtr( object );
	if (!wrp)
		return false;

	auto found = mMap.find( key );
	if( !replace && found != mMap.end() )
	{
		//the object was already in the map, and we weren't asked to replace it.
		return true;
	}

	Value* value;
	if( found == mMap.end() )
	{
		value = CCP_NEW( "MotherLode::Value" ) Value( this );
		value->mKey = key;
		if( inserted )
		{
			*inserted = true;
		}
		mMap[key] = value;
	}
	else
	{
		value = found->second;
	}

	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p, inserted at %S", wrp.p, key );
	}
	
	value->Setup(wrp);
	value->mAllowCaching = allowCaching;
	value->Assert();
	return true;
}

bool MotherLode::HasKey( const wchar_t* key )
{
	map_t::iterator it = mMap.find(key);
	return it != mMap.end();
}

IMotherLode::LookupResult MotherLode::Lookup(
	const wchar_t *key,
	const Be::IID& riid,
	void** ppv,
	BLUEQIOPT options
	)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	*ppv = 0;
	map_t::iterator it = mMap.find(key);
	if( it == mMap.end() )
	{
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Lookup failed for %S", key);
		}
		return LOOKUP_SUCCESS;
	}

	auto value = it->second;

	//get the interface
	if( !value->mWeak->QueryInterface(riid, ppv, options) )
	{
		return LOOKUP_FAILED; //yes, it was found, but QI failed.
	}

	//now, if we had a strong reference, throw it away and
	//turn it into a weak reference.
	bool fromCache = false;
	if( value->IsStrong() )
	{
		value->Uncache();
		value->Register();
		fromCache = true;
	}
	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p, Lookup succeeded %s at %S", value->mWeak, fromCache ? "from cache " : "", key );
	}

	return fromCache ? LOOKUP_CACHED : LOOKUP_SUCCESS;
}


bool MotherLode::Delete(const wchar_t *key)
{
	map_t::iterator it = mMap.find(key);
	if (it == mMap.end())
		return false;
	auto value = it->second;
	mMap.erase(it);
	CCP_DELETE value; //destructor will perform necessary cleanup
	return true;
}


void MotherLode::SetCacheSize(size_t mem)
{
	mMaxMemUsage = mem;
	Housekeeping();
}


size_t MotherLode::GetCacheSize()
{
	return mMaxMemUsage;
}


void MotherLode::GetStats(size_t *n_live, size_t *n_cached, size_t *c_mem)
{
	if (n_live)
		*n_live = mPending.size() + mLRU.size();
	if (n_cached)
		*n_cached = mMap.size() - mPending.size() - mLRU.size();
	if (c_mem)
		*c_mem = mMemUsage;
}


void MotherLode::Housekeeping()
{
	AssertAll();
	
	//first, fix up any pending caches
	for(list_t::iterator i = mPending.begin(); i!=mPending.end(); ) {
		list_t::iterator ii = i++;
		map_t::iterator j = mMap.find( *ii );
		auto value = j->second;
		value->Assert();
		CCP_ASSERT( value->IsStrong() && value->IsPending() );
		if ( value->mCacheable->IsMemoryUsageKnown() ) 
		{
			size_t s = value->mCacheable->GetMemoryUsage();
			value->Unlink();
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Object %p, size=%Iu", value->mWeak, s);
			}

			ssize_t newsize = mMemUsage + s;
			if (newsize < mMemUsage) {
				CCP_LOGWARN_CH( s_ml, "Object %p, MemUsage overflow, mMemUsage=%Id, size=%Iu", 
					value->mWeak, mMemUsage, s);
				CCP_LOGWARN_CH( s_ml, "key = %S", j->first.c_str());
				s = 0;
			} else if (mMaxMemUsage && s > (size_t)mMaxMemUsage) {
				if( mVerbose )
				{
					CCP_LOG_CH( s_ml, "Object %p, size %Iu larger than MaxMemUsage of %Id",
						value->mWeak, s, mMaxMemUsage);
					CCP_LOG_CH( s_ml, "key = %S", j->first.c_str());
				}

				s = 0;
			}
			if (s && mMaxMemUsage) {
				mMemUsage = newsize;
				value->mMemUsage = s;
				value->Link(); //link into the proper LRU list
			} else {
				//oh well, it doesn't have anything we need
				mMap.erase(j);
				CCP_DELETE value;
			}
		}
	}

	// then, trim from cache if necessary
	if( mVerbose && mMemUsage > mMaxMemUsage && !mLRU.empty() )
	{
		CCP_LOG_CH( s_ml, "Trimming memory usage from %Id to %Id", mMemUsage, mMaxMemUsage );
	}

	// then, trim from cache if necessary
	while( mMemUsage > mMaxMemUsage && !mLRU.empty() )
	{
		//we may need multiple passes.  Removing a strong ref may cause
		//a weak ref to the same object to become strong again!
		list_t::iterator front = mLRU.begin();
		map_t::iterator j = mMap.find( *front );
		if( j == mMap.end() )
		{
			// This shouldn't be happening, but we're seeing crashes in the erase
			// call below.
			CCP_LOGERR_CH( s_ml, "MotherLode: Iterator not found in map" );
			mLRU.erase( front );
			continue;
		}

		auto value = j->second;

		CCP_ASSERT( value->IsStrong() && !value->IsPending() );
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Object %p, clearing object with mem %Iu", value->mWeak, value->mMemUsage );
		}

		mMap.erase( j );
		CCP_DELETE value; //destructor calls Uncache, which modifies mMemUsage and removes the item from LRU list

		if( mMemUsage <= mMaxMemUsage )
		{
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Done trimming at %Id bytes", mMemUsage );
			}
		}
	}
	if( mLRU.empty() )
	{
		mMemUsage = 0; //for sanity
	}
}


void MotherLode::Startup()
{
	BeOS->RegisterForTicks(this, cookie);
	mActive = true;
}


void MotherLode::Shutdown()
{
	if (mActive)
	{
		mActive = false;
		Clear();
		if (BeOS)
			BeOS->UnregisterForTicks(this, cookie);
	}
}


//IBlueEvents interface
void MotherLode::OnTick(Be::Time realTime, Be::Time simTime, void *cookie)
{
	Housekeeping();
}


//the weak object is dying.  We now attempt to create a strong reference to it and
//cache it.
//Note that we must not unregister it.  An implicit unregistering has already
//been done by the caller (the WeakObject)
void MotherLode::WeakRefNotify( Value* v )
{
	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p at %S, attempting to cache", v->mWeak, v->mKey.c_str() );
	}

	//this object is dying
	//is it cacheable?
	CCP_ASSERT(!v->mCacheable);
	if (v->mAllowCaching == CACHING_ALLOWED && mMaxMemUsage)
		v->mWeak->QueryInterface(GetICacheableIID(), (void**)&v->mCacheable, BEQI_SILENT);

	if (!v->mCacheable) {
		//Not caching, or caching not supported
		if( mVerbose )
		{
			if (mMaxMemUsage)
				CCP_LOG_CH( s_ml, "Object %p, not cacheable", v->mWeak);
			else
				CCP_LOG_CH( s_ml, "Caching disabled");
		}

		v->mWeak = 0; //to disable an implicit Unregister call
		mMap.erase( v->mKey );
		CCP_DELETE v;
		return;
	}

	//find out its size, if it knows it.
	if (v->mCacheable->IsMemoryUsageKnown()) {
		size_t s = v->mCacheable->GetMemoryUsage();
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Object %p, size = %Iu", v->mWeak, s);
		}

		ssize_t newsize = mMemUsage + s;
		if (newsize < mMemUsage) {
			CCP_LOGWARN_CH( s_ml, "Object %p, MemUsage overflow, mMemUsage=%Id, size=%Iu", v->mWeak, mMemUsage, s);
			CCP_LOGWARN_CH( s_ml, "key = %S", v->mKey.c_str());
			s = 0;
		} else if (s > (size_t)mMaxMemUsage) {
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Object %p, size %Iu larger than MaxMemUsage of %Id",
					v->mWeak, s, mMaxMemUsage);
				CCP_LOG_CH( s_ml, "key = %S", v->mKey.c_str());
			}

			s = 0;
		}
		if (!s) {
			//we are not interested in this, zero memory
			v->mWeak = 0; //to disable an implicit Unregister call
			mMap.erase( v->mKey );
			CCP_DELETE v;
			return;
		}
		mMemUsage = newsize;
		v->mMemUsage = s;
	} else {
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Object %p size pending", v->mWeak);
		}

		v->mMemUsage = 0; //pending
	}
	
	//insert it at the end of the appropriate list
	v->Link();
}


void MotherLode::AssertAll() {
	for(map_t::iterator i = mMap.begin(); i!=mMap.end(); ++i)
		i->second->Assert();
}


void MotherLode::Clear() {
	//it is best to clear weak references first, so that clearing
	//strong references doesn't cause side effects in the map, if objects
	//are twice mapped.
	for(map_t::iterator i = mMap.begin(); i!=mMap.end(); ){
		map_t::iterator ii = i++;
		auto value = ii->second;
		if( !value->IsStrong() )
		{
			mMap.erase( ii );
			CCP_DELETE value;
		}
	}
	for( auto it = mMap.begin(); it != mMap.end(); ++it )
	{
		CCP_DELETE it->second;
	}
	mMap.clear();
	mLRU.clear();
	mPending.clear();
	mMemUsage = 0;
}

void MotherLode::ClearCached()
{
	for( map_t::iterator i = mMap.begin(); i!=mMap.end(); )
	{
		map_t::iterator ii = i++;
		auto value = ii->second;
		if( value->IsStrong() )
		{
			mMap.erase(ii);
			CCP_DELETE value;
		}
	}

	mLRU.clear();
	mPending.clear();
	mMemUsage = 0;
}

///////////////////////////////////////
// Python methods


#if BLUE_WITH_PYTHON

PyObject *MotherLode::Pyitems(PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":items"))
		return 0;
	BluePy r(PyList_New(mMap.size()));
	size_t i;
	map_t::iterator it;
	for(i=0, it=mMap.begin(); it!=mMap.end(); ++i, ++it) {
		PyObject *v = Py_BuildValue("NN",
			PyUnicode_FromWideChar(it->first.c_str(), it->first.size()),
			BlueWrapObjectForPython(it->second->mWeak)
			);
		if (!v)
			return 0;
		PyList_SET_ITEM(r.o, i, v);
	}
	return r.Detach();
}


PyObject *MotherLode::PynWeak(PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":nWeak"))
		return 0;
	return PyLong_FromSize_t(mMap.size() - mLRU.size() - mPending.size());
}


PyObject* MotherLode::PyLookupAsWeakRef(PyObject *args)
{
	PyObject *keyO;

	if( !PyArg_ParseTuple( args, "O:LookupAsWeakRef", &keyO ) )
	{
		return nullptr;
	}

	BluePy keyU(PyUnicode_FromObject(keyO));
	if( !keyU )
	{
		return nullptr;
	}

	wchar_t* keyStrBuffer = PyUnicode_AsWideCharString( keyU.o, NULL );
	std::wstring key( keyStrBuffer );
	PyMem_Free( keyStrBuffer );

	map_t::iterator it = mMap.find( key );
	if( it == mMap.end() )
	{
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Lookup failed for %S", key.c_str() );
		}
	}

	BluePythonWeakRefPtr wr;
	if( !wr.CreateInstance( GetBluePythonWeakRefClsid() ) )
	{
		return nullptr;
	}

	wr->SetObject( it->second->mWeak );

	return BlueWrapObjectForPython( wr );
}


PyObject *MotherLode::PyGetCachedKeys(PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetCachedKeys"))
		return 0;

	BluePy r(PyList_New(mLRU.size()));
	size_t i;
	list_t::iterator it;
	for(i=0, it=mLRU.begin(); it!=mLRU.end(); ++i, ++it) {
		PyObject *v = PyUnicode_FromWideChar(it->c_str(), it->size());
		PyList_SET_ITEM(r.o, i, v);
	}
	return r.Detach();
}


PyObject *MotherLode::PyGetNonCachedKeys(PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetNonCachedKeys"))
		return nullptr;

	size_t n = mMap.size() - mLRU.size() - mPending.size();
	BluePy r( PyList_New( n ) );
	
	size_t i;
	map_t::iterator it;
	for( i = 0, it = mMap.begin(); it != mMap.end(); ++it )
	{
		if( !it->second->IsStrong() )
		{
			// We want values that don't have a strong reference. Remember,
			// strong reference implies it cached (we're keeping it alive).
			PyObject *k = PyUnicode_FromWideChar(it->first.c_str(), it->first.size());
			if (!k)
			{
				PyErr_SetString( PyExc_AssertionError, "Key should be Unicode" );
				return nullptr;
			}
			PyList_SET_ITEM(r.o, i, k);

			++i;
		}
	}

	CCP_ASSERT( i == n );

	return r.Detach();
}

#endif

Be::Result<std::string> MotherLode::InsertFromScript( const std::wstring& key, IRoot* obj )
{
	if( !obj )
	{
		return Be::Result<std::string>( "Null object not allowed" );
	}

	bool inserted;
	bool ok = Insert( key.c_str(), obj, true, &inserted);
	if( !ok )
	{
		return Be::Result<std::string>( "Failed to insert object" );
	}
	return Be::Result<std::string>();
}

Be::Result<std::string> MotherLode::LookupFromScript( const std::wstring& key, IRoot** returnedObject )
{
	IRootPtr obj;
	bool ok = Lookup( key.c_str(), GetIRootIID(), (void**)&obj ) != LOOKUP_FAILED;
	if( !ok )
	{
		*returnedObject = nullptr;
		return Be::Result<std::string>( "Lookup failed" );
	}
	
	*returnedObject = obj.Detach();
	return Be::Result<std::string>();
}

bool MotherLode::DeleteFromScript( const std::wstring& key )
{
	return Delete( key.c_str() );
}

std::list<std::wstring> MotherLode::GetKeys()
{
	std::list<std::wstring> returnValue;
	for( auto it = mMap.begin(); it != mMap.end(); ++it )
	{
		returnValue.push_back( it->first );
	}

	return returnValue;
}

std::list<IRoot*> MotherLode::GetValues()
{
	std::list<IRoot*> returnValue;
	for( auto it = mMap.begin(); it != mMap.end(); ++it )
	{
		returnValue.push_back( it->second->mWeak );
	}

	return returnValue;
}

size_t MotherLode::GetSize()
{
	return mMap.size();
}


////////////////////////////////
// MotherLode::Value members


MotherLode::Value::Value(MotherLode *ml) :
	mMl(ml),
	mWeak(0),
	mMemUsage(0),
	mLinked(false),
	mAllowCaching(CACHING_ALLOWED)
{}


MotherLode::Value::~Value() {
	Unregister();
	Uncache();
}


//assignment only possible for unlinked dudes.  We also don't explicity register
void MotherLode::Value::Setup(IWeakObject *wo) {
	Unregister();
	Uncache();
	mWeak = wo;
	Register();
}


void MotherLode::Value::Register() {
	CCP_ASSERT(mWeak && !IsStrong());
	mWeak->WeakRefRegister(this);
}


//Should be called before Uncache
void MotherLode::Value::Unregister() {
	if (mWeak && !IsStrong()) //we are still a valid weakref
		mWeak->WeakRefUnregister(this);
}


void MotherLode::Value::Link() {
	CCP_ASSERT(!IsLinked());
	CCP_ASSERT(IsStrong());
	list_t &list = IsPending() ? mMl->mPending : mMl->mLRU;
	list.push_back(mKey);
	mLinked = true;
}	


void MotherLode::Value::Unlink() {
	if (!IsLinked())
		return;
	CCP_ASSERT(IsStrong());
	if (!IsPending())
		mMl->mLRU.erase( std::find( mMl->mLRU.begin(), mMl->mLRU.end(), mKey ) );
	else
		mMl->mPending.erase( std::find( mMl->mPending.begin(), mMl->mPending.end(), mKey ) );
	mLinked = false;
}


void MotherLode::Value::Uncache() {
	if (mCacheable) {
		if (IsLinked()) {
			Unlink();
			mMl->mMemUsage -= mMemUsage;
		}
		mMemUsage = 0;
		mCacheable.Unlock();
	} else {
		CCP_ASSERT(!IsLinked());
	}
}
		

void MotherLode::Value::WeakRefNotify(IWeakObject *ptr)
{
	CCP_ASSERT(ptr == mWeak);
	CCP_ASSERT(!mCacheable);
	mMl->WeakRefNotify( this );
}


void MotherLode::Value::Assert() const
{
	if (!IsStrong()) {
		CCP_ASSERT(!IsLinked());
	} else {
		CCP_ASSERT(IsLinked());
	}
}
