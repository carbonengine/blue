/* 
	*************************************************************************

	MotherLode.cpp

	Author:    Kristján Valur Jónsson
	Created:   Dec 2008
	OS:        Win32
	Project:   Yep

	Description:   

		A new general instance manager for blue objects.  Stores objects
		that support the IWeakRef interface, so that one can access them
		via a unicode key while they are alive.
		Additionally, if they support the ICaceable interface, they will
		Be stored for an additional amount of time (cached) if depending
		on the amount of memory that they consume.

	Dependencies:

		Blue

	(c) CCP 2008

	*************************************************************************
*/
#include "StdAfx.h"

#include "MotherLode.h"
#if BLUE_WITH_PYTHON
#include "BluePythonWeakRef.h"
#endif

IMotherLode* BeMotherLode = nullptr;
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

	std::pair<map_t::iterator, bool> ins = mMap.insert(map_t::value_type(key, this));
	if(!replace && !ins.second)
		//the object was already in the map, and we weren't asked to replace it.
		return true;
	
	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p, inserted at %S", wrp.p, key );
	}
	
	if (ins.second)
	{
		//insert the iterator pointing to our place
		ins.first->second.mKey = key;
	}
	ins.first->second.Setup(wrp);
	ins.first->second.mAllowCaching = allowCaching;
	if (inserted)
		*inserted = true;
	ins.first->second.Assert();
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

	//get the interface
	if( !it->second.mWeak->QueryInterface(riid, ppv, options) )
	{
		return LOOKUP_FAILED; //yes, it was found, but QI failed.
	}

	//now, if we had a strong reference, throw it away and
	//turn it into a weak reference.
	bool fromCache = false;
	if( it->second.IsStrong() )
	{
		it->second.Uncache();
		it->second.Register();
		fromCache = true;
	}
	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p, Lookup succeeded %sat %S", it->second.mWeak, fromCache ? "from cache " : "", key );
	}

	return fromCache ? LOOKUP_CACHED : LOOKUP_SUCCESS;
}


bool MotherLode::Delete(const wchar_t *key)
{
	map_t::iterator it = mMap.find(key);
	if (it == mMap.end())
		return false;
	mMap.erase(it); //destructor will perform necessary cleanup
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
		j->second.Assert();
		CCP_ASSERT(j->second.IsStrong() && j->second.IsPending());
		if (j->second.mCacheable->IsMemoryUsageKnown()) {
			size_t s = j->second.mCacheable->GetMemoryUsage();
			j->second.Unlink();
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Object %p, size=%Iu", j->second.mWeak, s);
			}

			ssize_t newsize = mMemUsage + s;
			if (newsize < mMemUsage) {
				CCP_LOGWARN_CH( s_ml, "Object %p, MemUsage overflow, mMemUsage=%Id, size=%Iu", 
					j->second.mWeak, mMemUsage, s);
				CCP_LOGWARN_CH( s_ml, "key = %S", j->first.c_str());
				s = 0;
			} else if (mMaxMemUsage && s > (size_t)mMaxMemUsage) {
				if( mVerbose )
				{
					CCP_LOG_CH( s_ml, "Object %p, size %Iu larger than MaxMemUsage of %Id",
						j->second.mWeak, s, mMaxMemUsage);
					CCP_LOG_CH( s_ml, "key = %S", j->first.c_str());
				}

				s = 0;
			}
			if (s && mMaxMemUsage) {
				mMemUsage = newsize;
				j->second.mMemUsage = s;
				j->second.Link(); //link into the proper LRU list
			} else {
				//oh well, it doesn't have anything we need
				mMap.erase(j);
			}
		}
	}

	// then, trim from cache if necessary
	while (mMemUsage > mMaxMemUsage && mLRU.size()) {
		//we may need multiple passes.  Removing a strong ref may cause
		//a weak ref to the same object to become strong again!
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Trimming memory usage from %Id to %Id", mMemUsage, mMaxMemUsage);
		}

		for(list_t::iterator i=mLRU.begin(); i!=mLRU.end(); ) {
			list_t::iterator ii = i++;
			map_t::iterator j = mMap.find( *ii );

			CCP_ASSERT(j->second.IsStrong() && !j->second.IsPending());
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Object %p, clearing object with mem %Iu", j->second.mWeak, j->second.mMemUsage);
			}

			mMap.erase(j); //destructor calls Uncache, which modifies mMemUsage
			if (mMemUsage <= mMaxMemUsage) {
				if( mVerbose )
				{
					CCP_LOG_CH( s_ml, "Done trimming at %Id bytes", mMemUsage);
				}
				break;
			}
		}
	}
	if (!mLRU.size())
		mMemUsage = 0; //for sanity
}


void MotherLode::Startup()
{
	BeOS->RegisterForTicks(this, cookie);
	mActive = true;
}


void MotherLode::Shutdown()
{
	CCP_ASSERT(mActive);
	mActive = false;
	Clear();
	if (BeOS)
		BeOS->UnregisterForTicks(this, cookie);
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
void MotherLode::WeakRefNotify( Value& v )
{
	if( mVerbose )
	{
		CCP_LOG_CH( s_ml, "Object %p at %S, attempting to cache", v.mWeak, v.mKey.c_str() );
	}

	//this object is dying
	//is it cacheable?
	CCP_ASSERT(!v.mCacheable);
	if (v.mAllowCaching == CACHING_ALLOWED && mMaxMemUsage)
		v.mWeak->QueryInterface(GetICacheableIID(), (void**)&v.mCacheable, BEQI_SILENT);

	if (!v.mCacheable) {
		//Not caching, or caching not supported
		if( mVerbose )
		{
			if (mMaxMemUsage)
				CCP_LOG_CH( s_ml, "Object %p, not cacheable", v.mWeak);
			else
				CCP_LOG_CH( s_ml, "Caching disabled");
		}

		v.mWeak = 0; //to disable an implicit Unregister call
		mMap.erase( v.mKey );
		return;
	}

	//find out its size, if it knows it.
	if (v.mCacheable->IsMemoryUsageKnown()) {
		size_t s = v.mCacheable->GetMemoryUsage();
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Object %p, size = %Iu", v.mWeak, s);
		}

		ssize_t newsize = mMemUsage + s;
		if (newsize < mMemUsage) {
			CCP_LOGWARN_CH( s_ml, "Object %p, MemUsage overflow, mMemUsage=%Id, size=%Iu", v.mWeak, mMemUsage, s);
			CCP_LOGWARN_CH( s_ml, "key = %S", v.mKey.c_str());
			s = 0;
		} else if (s > (size_t)mMaxMemUsage) {
			if( mVerbose )
			{
				CCP_LOG_CH( s_ml, "Object %p, size %Iu larger than MaxMemUsage of %Id",
					v.mWeak, s, mMaxMemUsage);
				CCP_LOG_CH( s_ml, "key = %S", v.mKey.c_str());
			}

			s = 0;
		}
		if (!s) {
			//we are not interested in this, zero memory
			v.mWeak = 0; //to disable an implicit Unregister call
			mMap.erase( v.mKey );
			return;
		}
		mMemUsage = newsize;
		v.mMemUsage = s;
	} else {
		if( mVerbose )
		{
			CCP_LOG_CH( s_ml, "Object %p size pending", v.mWeak);
		}

		v.mMemUsage = 0; //pending
	}
	
	//insert it at the end of the appropriate list
	v.Link();
}


void MotherLode::AssertAll() {
	for(map_t::iterator i = mMap.begin(); i!=mMap.end(); ++i)
		i->second.Assert();
}


void MotherLode::Clear() {
	//it is best to clear weak references first, so that clearing
	//strong references doesn't cause side effects in the map, if objects
	//are twice mapped.
	for(map_t::iterator i = mMap.begin(); i!=mMap.end(); ){
		map_t::iterator ii = i++;
		if (!ii->second.IsStrong())
			mMap.erase(ii);
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
		if( ii->second.IsStrong() )
		{
			mMap.erase(ii);
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
			PyUnicode_FromUnicode((Py_UNICODE*)it->first.c_str(), it->first.size()),
			BlueWrapObjectForPython(it->second.mWeak)
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
	return PyInt_FromSize_t(mMap.size() - mLRU.size() - mPending.size());
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

	std::wstring key = (const wchar_t*)PyUnicode_AS_UNICODE(keyU.o);
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

	wr->SetObject( it->second.mWeak );

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
		PyObject *v = PyUnicode_FromUnicode((Py_UNICODE*)it->c_str(), it->size());
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
		if( !it->second.IsStrong() )
		{
			// We want values that don't have a strong reference. Remember,
			// strong reference implies it cached (we're keeping it alive).
			PyObject *k = PyUnicode_FromUnicode((Py_UNICODE*)it->first.c_str(), it->first.size());
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
		returnValue.push_back( it->second.mWeak );
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
	mMl->WeakRefNotify( *this );
}


void MotherLode::Value::Assert() const
{
	if (!IsStrong()) {
		CCP_ASSERT(!IsLinked());
	} else {
		CCP_ASSERT(IsLinked());
	}
}
