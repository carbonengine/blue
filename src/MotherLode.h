// Copyright © 2008 CCP ehf.

/*
	*************************************************************************

	MotherLode.h

	Description:

		A new general instance manager for blue objects.  Stores objects
		that support the IWeakObject interface, so that one can access them
		via a unicode key while they are alive.
		Additionally, if they support the ICacheable interface, they will
		be stored for an additional amount of time (cached) if depending
		on the amount of memory that they consume.

	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _MOTHERLODE_H_
#define _MOTHERLODE_H_

#include "IMotherLode.h"
#include "IBlueOS.h"


//This class implements the MotherLode.
//It is basically a WeakValue dictionary, where one can Insert objects (that support the
//IWeakObject interface) into it.  In addition, when a weak reference dies, the object
//may get a strong ref if it supports the ICacheable interface.  In this case the MotherLode
//creates a strong referecen to it.  It discards those strong referenced objects on an oldest-
//first basis if the memory used by those objects exceeeds a limit.
//If a strongly referenced objects is looked up in the MotherLode, the strong reference is
//thrown away and a new weak reference created again.
//Thus, the only objects in the cache are objects not used by anyone else.

//To implement this, we use a hash map to find weak objects by the unicode key.
//If an object is strongly referenced, an iterator to the map is also inserted into
//a list.
//If the size of the cached object isn't know at the time it becomes strong, it goes
//into a special pending list, whence it is removed once the size is known.  This is to
//cater to background loading of resources.

BLUE_DECLARE(MotherLode);

class MotherLode :
	public IMotherLode,
	public IBlueEvents
{
public:
	EXPOSE_TO_BLUE();

	MotherLode( IRoot* lockobj = nullptr );
	~MotherLode();

	//the IMotherLode interface
	bool Insert(
		const wchar_t*	key,
		IRoot*			object,
		bool			replace = true,
		bool			*inserted = 0,
		ResourceCaching	allowCaching = CACHING_ALLOWED
		) override;

	bool HasKey( const wchar_t* key ) override;

	LookupResult Lookup(
		const wchar_t *key,
		const Be::IID& riid,
		void** ppv,
		BLUEQIOPT options = BEQI_NONE
		) override;

	bool Delete(const wchar_t *key) override;
	void SetCacheSize(size_t size) override;
	size_t GetCacheSize() override;
	void GetStats(size_t *n_live, size_t *n_cached, size_t *c_mem) override;
	void Startup() override;
	void Shutdown() override;


	//The IBlueEvents interface
	void OnTick(
		Be::Time realTime,		// Time since client started
		Be::Time simTime,
		void* cookie			// user supplied cookie value
		) override;

	// Clear all objects.
	void Clear();

	// Clear cached objects, that is only objects that have no other strong references.
	void ClearCached();

	Be::Result<std::string> InsertFromScript( const std::wstring& key, IRoot* obj );
	Be::Result<std::string> LookupFromScript( const std::wstring& key, IRoot** obj );
	bool DeleteFromScript( const std::wstring& key );
	std::list<std::wstring> GetKeys();
	std::list<IRoot*> GetValues();
	size_t GetSize();

#if BLUE_WITH_PYTHON
	PyObject* Pyitems( PyObject* args );
	PyObject* PynWeak( PyObject* args );
	PyObject* PyLookupAsWeakRef( PyObject* args );
	PyObject* PyGetCachedKeys( PyObject* args );
	PyObject* PyGetNonCachedKeys( PyObject* args );
#endif

private:

	// The value stored in the map.  Can hold either a weakref, or a strongref.
	struct Value:
		public IWeakRef
	{
		Value(MotherLode *ml);
		virtual ~Value();

		//Assert various things
		void Assert() const;

		// The IWeakRef interface
		virtual void WeakRefNotify(IWeakObject *ptr);

		void Setup(IWeakObject *wo);

		//Register and unregistering the weakref.  Unregister is not
		//necessary after a WeakRefNotify call.
		void Register();
		void Unregister(); //call becore Uncache

		//Linking the value into a list, for LRU or Pending lookup
		void Link();
		void Unlink();
		void Uncache(); //deletes the strong reference and unlinks

		bool IsStrong() const {return mCacheable != 0;}
		bool IsPending() const {CCP_ASSERT(IsStrong()); return mMemUsage == 0;}
		bool IsLinked() const {return mLinked; }

		std::wstring mKey;
		MotherLode *mMl;		//the owning motherlode
		IWeakObject *mWeak;		//the object being managed
		BluePtr<ICacheable> mCacheable;	//the strong reference we hold
		size_t mMemUsage;		//size of the memory, 0 if pending
		//There is no null iterator defined in STL so we need a flag to see if
		//mList is valid.  Also, cannot use e.g. mMl->mLRU.end() as a marker because
		//it is forbidden (and assertions in place) to compare iterators from
		//different lists.
		bool	mLinked;		//is mList valid?
		// Allow or disable caching for this resource
		ResourceCaching mAllowCaching;
	};
	friend Value;

	void AssertAll();
	void WeakRefNotify( Value* value );
	void Housekeeping();

	typedef TrackableStdHashMap<std::wstring, Value*> map_t;
	typedef TrackableStdList<std::wstring> list_t;

	// key-value map. The value holds the actual object, either as a strong
	// reference (for objects that are cached) or as a weak reference when
	// objects are in use.
	map_t mMap;

	// Cached objects in LRU order. Note that the objects also live in the map.
	list_t mLRU;

	// Cached objects with pending size info, i.e. asynchronously loaded resources
	// that haven't finished loading.
	list_t mPending;

	// The total memory usage for cached objects
	ssize_t mMemUsage;

	// Allowed memory usage for cached objects
	ssize_t mMaxMemUsage;

	bool mActive;	//is this turned on?
	bool mVerbose;
};

TYPEDEF_BLUECLASS_WR(MotherLode);

#endif // !defined _MOTHERLODE_H_
