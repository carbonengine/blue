#include "StdAfx.h"

#if !USE_RESFILE_2

#include "include/IMotherLode.h"
#include "BlueResFile.h"
#include "EmbedFs.h"
#include "Stuffer.h"
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <string.h>
#include <cwctype>
#include <errno.h>

#include "include/BlueFileUtil.h"
#include "include/IBluePaths.h"

#ifdef __ANDROID__
#include <sys/stat.h>
#endif

#if BLUE_WITH_PYTHON
#ifdef _WIN32
#include <CcpUtils/PyCpp.h>
using Ccp::PyAllowThreads;
#endif
#endif

static CcpLogChannel_t s_chFile = CCP_LOG_DEFINE_CHANNEL( "File" );

// TODO: replace with a global g_blueVerbose flag
static bool s_verbose = true;

#define INVALID_SET_FILE_POINTER ((DWORD)-1)


ResFile::ResFile() 
	:mFilename(L""),
	mAscii( false )
{
#if STUFFER_ENABLED
	if( !BeStuffer )
	{
		CCP_LOGWARN_CH( s_chFile, "ResFile created before Stuffer is started" );
		Stuffer::Startup();
	}
#endif

	mData = NULL;
	mSize = 0;

	mLockCount = 0;
	
#ifdef _WIN32
	mFile = INVALID_HANDLE_VALUE;
	mMapping = NULL;
#else
	mFile = nullptr;
#endif

	mPosition = 0;
	mReadOnly = true;
	mStuffHandle = -1;
	mSoftspace = 0;
#if RESFILE_PRELOADING_SUPPORTED
	mPreloadCount = 0;
#endif
}


ResFile::~ResFile()
{
	Close();
}


bool ResFile::IsOpen() const
{
#ifdef _WIN32
	if (mFile == INVALID_HANDLE_VALUE && !mData)
	{
		BeOS->SetError(BEDEF, Clsid(), "No file open");
		return false;
	}
	else
	{
		return true;
	}
#else
	return mFile != nullptr;
#endif
}


bool ResFile::OpenW(
	const wchar_t* filename,
	bool readOnly
	)
{
	if( s_verbose )
	{
		CCP_LOG_CH( s_chFile, "Open %S", filename );
	}

	bool error;
	return ResFile::OpenWImpl( error, filename, readOnly, false, false );
}

bool ResFile::OpenWImpl(
	bool &error,
	const wchar_t* filename,
	bool readOnly,
	bool exists,
	bool silent //don't treat non-existence as an error
	)
{
	error = false;
	if (!exists) {
		//close the file first
		if (!Close()) {
			error = true;
			return false;
		}
	}
	bool isRes = !wcsncmp(filename, L"res:", 4);
	
	bool tryStuffer = isRes && readOnly; //stuffers are only for readonly

	//FS is for all but stuff, except, when not packaged, then it is for res too.
	// When packaged, the 'resFromStuffOnly' flag in boot.ini controls whether
	// file system is allowed, or stuff files only for res paths.
#if BLUE_WITH_PYTHON
	bool tryFS = !isRes || !(PyOS->IsPackaged() && PyOS->IsResFromStuffOnly()); 
#else
	// TODO: Clean this up
	bool tryFS = true;
#endif

	bool tryLang = false; //no separate language try
	std::wstring filenameToOpen = filename;
	std::wstring tmpfilename = filename; //used when doing language mangling

	if( isRes )
	{
		tryLang = AdjustFilenameForLanguageCode( filenameToOpen, tmpfilename );
	}

	//Now, try the stuffer
    bool found = false;
	if( tryStuffer )
	{
        if( tryLang )
		{
			found = OpenWStuff(tmpfilename.c_str(), exists);
		}
        if( !found )
		{
			found = OpenWStuff(filenameToOpen.c_str(), exists);
        }
	}

	if( !found )
	{
		if( tryFS )
		{
			if( tryLang )
			{
				found = OpenWFS(error, tmpfilename.c_str(), exists, true, readOnly);
			}
			if( !found && !error)
			{
				found = OpenWFS(error, filenameToOpen.c_str(), exists, silent, readOnly);
			}
		}
		else
		{
			if( !exists && !silent )
			{
				BeOS->SetError( BE32, Clsid(), "Open failed on \"%S\"", filename );
				error = true;
			}
        }
	}

	if( found && !exists )
	{
		mFilename = filename;
    }
	
	return found;
}

bool ResFile::OpenWStuff(const wchar_t *fn, bool exists)
{
#if STUFFER_ENABLED
	std::wstring tmp(fn + 4); //cut off the "res:" prefix
	NormalizeSlashes(tmp);
	fn = tmp.c_str();

	if (exists)
		return BeStuffer->HasData(fn);

	bool ok = BeStuffer->LockData(fn, &mData, &mStuffHandle, &mSize);
	if (ok) {
		mFile = INVALID_HANDLE_VALUE;
		mPosition = 0;
		mReadOnly = true; //stuff is always readonly
	}
	return ok;
#else
	return false;
#endif
}


#ifdef _WIN32
bool ResFile::OpenWFS(bool &error, const wchar_t *filename, bool exists, bool silent, bool readOnly)
{

	std::wstring fn = BePaths->ResolvePathW(filename);

	HANDLE file;
	DWORD size, sizeHigh;
	error = false;
	{
#if BLUE_WITH_PYTHON
		PyAllowThreads _allow(true);
#endif
		file = CreateFileW(
								fn.c_str(),
								readOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, //seq scan boosts performance
								NULL
								);
		if (file == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();
			if ((!silent && !exists)  || (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND && err != ERROR_INVALID_NAME))
				goto ERR1;
			return false;
		}
		if (exists) {
			CloseHandle(file);
			return true;
		}

		size = GetFileSize(file, &sizeHigh);
		if (size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
			goto ERR2;
		
	}
#ifdef _WIN64
	LARGE_INTEGER s;
	s.LowPart = size;
	s.HighPart = sizeHigh;
	mSize = s.QuadPart;
#else
	if (sizeHigh) {
		BeOS->SetError(BEDEF, Clsid(), "file \"%s\" too large, sizehigh = %d", (char*)CW2A(fn.c_str(), CP_UTF8), sizeHigh);
		error = true;
		return false;
	}
	mSize = size;
#endif
	mFile = file;
	mReadOnly = readOnly;

	//Memory map the file right away, if it is readonly
	if (readOnly) {
		bool result = LockData(0, size);
		error = !result;
		return result;
	}
	return true;
ERR1:
	BeOS->SetError(BE32, Clsid(), "Open failed on \"%s\"", (char*)CW2A(fn.c_str(), CP_UTF8));
	error = true;
	return false;
ERR2:
	BeOS->SetError(BE32, Clsid(), "GetFileSize failed on \"%s\", handle %p", (char*)CW2A(fn.c_str(), CP_UTF8), (void*)file);
	error = true;

	return false;
}

#else

bool ResFile::OpenWFS(bool &error, const wchar_t *filename, bool exists, bool silent, bool readOnly)
{
	std::wstring fn = BePaths->ResolvePathW( filename );
	
	const char* mode;
	if( readOnly )
	{
		mode = "rb";
	}
	else
	{
		mode = "r+b";
	}
    
	std::string fnA = (const char*)CW2A( fn.c_str() );
	mFile = fopen( fnA.c_str(), mode );
    
	if( mFile )
	{
#ifdef __ORBIS__
		SceKernelStat statData;
		int statResult = sceKernelStat( fnA.c_str(), &statData );
#else
		struct stat statData;
		int statResult = stat( fnA.c_str(), &statData );
#endif
		if( statResult == 0 )
		{
			mSize = size_t( statData.st_size );
		}
	}
	else
	{
		char* errorMessage = strerror( errno );
		BeOS->SetError(BE32, Clsid(), "Open failed on \"%s\", errno=%d, %s", fnA.c_str(), errno, errorMessage );
	}
    
	return mFile != nullptr;
}

#endif

bool ResFile::Open(
	const char* filename,
	bool readOnly
	)
{
	return OpenW(CA2W(filename), readOnly);
}
	
#if RESFILE_PRELOADING_SUPPORTED

DWORD ResFile::PreloadThreadThunk(void *arg)
{
	ResFile *self = reinterpret_cast<ResFile*>(arg);
	self->PreloadThread();
	return 0;
}

DWORD ResFile::PreloadThread()
{
	// It's OK to use Telemetry functions naked on a background thread.
	// Don't do this if there is the slightest chance of it being called
	// on a tasklet.
	tmThreadName( g_telemetryContext, GetCurrentThreadId(), "PreloadThread" );
	tmEnter( g_telemetryContext, TMZF_NONE, __FUNCTION__ );

	_ASSERT(mData);
	_ASSERT(mPreloadCount == 1);
	size_t pagesize;
	SYSTEM_INFO sinfo;
	GetSystemInfo(&sinfo);
	pagesize = sinfo.dwPageSize;
	if (!pagesize)
		pagesize = 4*1024; //4 k

	//read one byte from each page
	//TODO: INvestigate if we should maybe do this backwards,
	//so that the first page (last read) is the fresheset in the page cache.
	DWORD sum = 0;
	char *p = (char*)mData;

#ifdef BENCHMARK
	LARGE_INTEGER start, end, freq;
	QueryPerformanceCounter(&start);
#endif
	__try
	{
		for (size_t i = 0; i<mSize; i+=pagesize)
			sum += p[i];
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
	   // Failed to read from the view.
	}
#ifdef BENCHMARK
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&freq);
	double time = (double)(end.QuadPart-start.QuadPart) / (double)freq.QuadPart;
	char tmpstr[256];
	DWORD id = GetCurrentThreadId();
	sprintf(tmpstr, "Preload, thread %8x, %12d bytes, %gms\n", id, mSize, time*1000.0);
	OutputDebugString(tmpstr);
#endif

	//The main thread must wait for us to be done, since this worker has
	//no strong reference to the main thread.
	mPreloadCount = -1;  //we are done.  Preload is compared only once.
	//Wake up the main thread if it is sleeping
	BeOS->NextScheduledEvent(0);

	tmLeave( g_telemetryContext );
	return sum;
}


void ResFile::PreloadWait()
{
	//wait until a worker has reset the mPreloadCount
	while(mPreloadCount == 1)
		Sleep(0);
}
#endif

//Start a preload If a file hass been mapped into view, start a thread
//that touches all the pages and so causes them to move into memory.
bool ResFile::Preload(bool &started)
{
#if RESFILE_PRELOADING_SUPPORTED
	CCP_STATS_ZONE( __FUNCTION__ );

	started = false;
	if (!mData)
		return true; //nothing to preload
	//increase the load count on the main thread
	LONG old = InterlockedCompareExchange(&mPreloadCount, 1, 0);
	if (old != 0)
		return true;//we are already preloading, or have preloaded once

	//Since the threads will not do much work, but mostly wait for IO, we
	//set this flag, so that windows won't wait for their completion but
	//rather start new threads as needed.
	ULONG flags = WT_EXECUTELONGFUNCTION;

	BOOL r = QueueUserWorkItem(PreloadThreadThunk, (void*) this, flags);
	if (!r) {
		mPreloadCount = 0; //reset preload status;
		BeOS->SetError(BE32, Clsid(), "QueueUserWorkItem failed");
		return false;
	}
	started = true;
	return true;
#else
	return false;
#endif
}


//Returns false if a preload is in progress.  This can be used to delay
//loading of resources, for excample
bool ResFile::PreloadInProgress()
{
#if RESFILE_PRELOADING_SUPPORTED
	return mPreloadCount == 1;
#else
	return false;
#endif
}


//--------------------------------------------------------------------
// IResFile::Create
//--------------------------------------------------------------------
bool ResFile::CreateW(
	const wchar_t* filename
	)
{
	if (!Close())
		return false;

	std::wstring tmp = BePaths->ResolvePathW(filename);
	mFilename = tmp.c_str();
	
#ifdef _WIN32
	HANDLE file;
	{
#if BLUE_WITH_PYTHON
		PyAllowThreads _allow(true);
#endif
		file = CreateFileW(
				mFilename.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				NULL
				);
	}
	
	if (file == INVALID_HANDLE_VALUE)
	{
		BeOS->SetError(BE32, Clsid(), "Create failed on \"%S\"", mFilename.c_str() );
		return false;
	}

	mFile = file;
	mReadOnly = false;

	return true;
#else

	const char* mode = "w+";

	std::string fnA = (const char*)CW2A( mFilename.c_str() );
	mFile = fopen( fnA.c_str(), mode );

	if( !mFile )
	{
		char* errorMessage = strerror( errno );
		BeOS->SetError(BE32, Clsid(), "Create failed on \"%s\", errno=%d, %s", fnA.c_str(), errno, errorMessage );
		return false;
	}

	mReadOnly = false;

	return true;
#endif
}	



//--------------------------------------------------------------------
// IResFile::Close
//--------------------------------------------------------------------
bool ResFile::Close()
{
#if RESFILE_PRELOADING_SUPPORTED
	PreloadWait();
#endif

	if( mData && mLockCount > 0 )
	{
		//force unlocking
		mLockCount = 1;
		UnlockData();
	}	

#ifdef _WIN32

	//Two cases, either a file or a stuffer (or none, in case of error)
	if (mFile != INVALID_HANDLE_VALUE)
		CloseHandle(mFile);
#if STUFFER_ENABLED
	else if (mData)
		BeStuffer->UnlockData(mData, mStuffHandle);
#endif

	mFile = INVALID_HANDLE_VALUE;

#else

	if( mFile )
	{
		fclose( mFile );
		mFile = nullptr;
	}

#endif

	mData = 0;
	mSize = 0;
	mPosition = 0;
	mStuffHandle = -1;
	mReadOnly = true;


	return true;
}


//--------------------------------------------------------------------
// IResFile::FileExists
//--------------------------------------------------------------------
bool ResFile::FileExistsW(
	const wchar_t* filename
	)
{
	bool error;
	const bool ret = OpenWImpl(error, filename, true, true, true);

	if( s_verbose )
	{
		CCP_LOG_CH( s_chFile, "FileExists %S : %s", filename, ret ? "yes" : "no" );
	}

	return ret;
}


//////////////////////////////////////////////////////////////////////
//
// IBlueStream interface methods
//
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
// IBlueStream::Read
//--------------------------------------------------------------------
#ifdef _WIN32
static bool safememcpy(void *dest, const void *src, size_t size)
{
	__try
	{
		memcpy(dest, src, size);
		return true;
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return false;
	}
}
#else
static bool safememcpy(void *dest, const void *src, size_t size)
{
	memcpy(dest, src, size);
	return true;
}

#endif

	
ssize_t ResFile::Read( void* dest, ssize_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );
	
	if( count < 0 || mPosition + count > mSize )
	{
		count = mSize - mPosition;
	}
	CCP_ASSERT(count >= 0);

	if (mData)
	{
		//don't release the python gil here.  We are reading from a memory mapped
		//file and this is extremely performance critical.
		bool ok = safememcpy(dest, (char*)mData + mPosition, count);
		if (!ok)
		{
			// Failed to read from the view.
#ifdef _WIN32
			SetLastError(ERROR_READ_FAULT);
#endif
			BeOS->SetError(BE32, Clsid(), "Couldn't Read");
			return -1;
		}
		//readcounters were updated when the file was mapped.
		mPosition += count;
		return count;
	}

	if (!IsOpen())
		return -1;

#ifdef _WIN32
	DWORD read;
	BOOL ok;
	{
#if BLUE_WITH_PYTHON
		PyAllowThreads _allow(true);
#endif
		ok = ReadFile(mFile, dest, (DWORD)count, &read, NULL);
	}
	if (!ok) {
		BeOS->SetError(BE32, Clsid(), "Couldn't Read");
		return -1;
	}
	
	mPosition += read;
	return read;
#else
	size_t bytesRead = fread( dest, 1, count, mFile );
	return bytesRead;
#endif
}


//--------------------------------------------------------------------
// IBlueStream::Write
//--------------------------------------------------------------------
ssize_t ResFile::Write(
	const void* source,
	size_t count
	)
{
	CCP_ASSERT( count >= 0 );

	if (!IsOpen())
		return -1;
	if (mReadOnly) {
		BeOS->SetError(BE32, Clsid(), "ResFile is read only");
		return -1;
	}

#ifdef _WIN32
	DWORD wrote;
	if (!WriteFile(mFile, source, (DWORD)count, &wrote, NULL))
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Write");
		return -1;
	}
#else
	size_t wrote = fwrite( source, 1, count, mFile );
	if( wrote < count )
	{
		char* errorMessage = strerror( errno );
		BeOS->SetError(BE32, Clsid(), "Write failed, errno=%d, %s", errno, errorMessage );
		return -1;
	}
#endif

	mPosition += wrote;
	
	if (mSize < mPosition)
		mSize = mPosition;

	return wrote;
}

//--------------------------------------------------------------------
// IBlueStream::Seek
//--------------------------------------------------------------------
ssize_t ResFile::Seek(
	ssize_t distance,
	BLUESEEK method
	)
{

	if (!IsOpen())
	{
		BeOS->SetError(BE32, Clsid(), "File not open");
		return -1;
	}

	if (mData)
	{
        // TODO: check if resulting position would be less than 0
		if (method == BS_BEGIN)
			mPosition = distance;
		else if (method == BS_CURRENT)
			mPosition += distance;
		else
		{
			// Standard python seek from the end takes a negative distance 
			// (not a positive one) like here
			mPosition = mSize - distance;
		}

		//truncate position
		if (mPosition > mSize)
			mPosition = mSize;

		return mPosition;
	}

#ifdef _WIN32
	DWORD ret = SetFilePointer(mFile, (DWORD)distance, NULL, method);

	if (ret == INVALID_SET_FILE_POINTER)
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Seek");
		return -1;
	}
	else
	{
		mPosition = ret;
		return ret;
	}

#else

	int origin = SEEK_SET;
	switch( method )
	{
		case BS_BEGIN:
			origin = SEEK_SET;
			break;

		case BS_CURRENT:
			origin = SEEK_CUR;
			break;

		case BS_END:
			origin = SEEK_END;
			break;

		default:
			origin = SEEK_SET;
			break;
	}
	int seekResult = fseek( mFile, distance, origin );
	if( seekResult == 0 )
	{
		mPosition = ftell( mFile );
		return mPosition;
	}
	else
	{
		return -1;
	}

#endif
}


//--------------------------------------------------------------------
// IBlueStream::GetPosition
//--------------------------------------------------------------------
ssize_t ResFile::GetPosition(
	)
{
	return mPosition;
}


//--------------------------------------------------------------------
// IBlueStream::GetSize
//--------------------------------------------------------------------
ssize_t ResFile::GetSize(
	)
{
	return mSize;
}


//--------------------------------------------------------------------
// IBlueStream::LockData
//--------------------------------------------------------------------
bool ResFile::LockData(
	void** data,
	size_t size
	)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (size && size != mSize)
	{
		BeOS->SetError(
			BEDEF, 
			Clsid(), 
			"LockData called with size=%d, must be either 0 or %d.",
			size, mSize
			);
	
		return false;
	}
	size = mSize;
	if (data)
		*data = 0;

#ifdef _WIN32

	if (!mData) {
		if (size) {
			//Todo: support Huge files (64 bit)
			//this is a file that needs mapping or loading
			//Set up a mapping
			mMapping = CreateFileMapping( mFile, NULL,
				mReadOnly ? PAGE_READONLY : PAGE_READWRITE,
				0, (DWORD)size, NULL);
			if (!mMapping) {
				BeOS->SetError(BE32, Clsid(), "LockData failed on CreateFileMapping, file %S",
					mFilename.c_str());
				return false;
			}

			mData = MapViewOfFile(
				mMapping,
				mReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE,
				0, 0,
				size
				);

			if (!mData)
			{
				BeOS->SetError(BE32, Clsid(), "LockData failed on MapViewOfFile, file %S", mFilename.c_str() );
				CloseHandle(mMapping);
				mMapping = NULL;
				return false;
			}
		} else {
			//create a dummy mapping to a null size object
			static int dummy = 0;
			mData = &dummy;
		}
	}
#else
	if( !mData )
	{
		mData = CCP_MALLOC( "ResFile/mData", mSize );
		rewind( mFile );
		size_t bytesRead = fread( mData, 1, mSize, mFile );
		if( bytesRead < mSize )
		{
			BeOS->SetError(BE32, Clsid(), "LockData failed to read, file %S", mFilename.c_str() );
			CCP_FREE( mData );
			mData = nullptr;
		}
	}
#endif

	if (data)
		*data = mData;
	
	if (mData) {
		mLockCount++;
		return true;
	}

	return false;
}


//--------------------------------------------------------------------
// IBlueStream::GetSize
//--------------------------------------------------------------------
bool ResFile::UnlockData(
	)
{
#ifdef _WIN32
	PreloadWait();
#endif

	mLockCount--;
	if (mLockCount)
		return true;

	if (mLockCount<0) {
		BeOS->SetError(BE32, Clsid(), "To Many UnlockData() calls on resfile");
		return false;
	}
	
	CCP_ASSERT(mData);

#ifdef _WIN32
	if (mMapping) {
		UnmapViewOfFile(mData);		
		CloseHandle(mMapping);
		mMapping = NULL;
		mData = 0;
	} else if (mStuffHandle == -1) {
		//it is a dummy mapping of a zero size
		_ASSERT(mSize == 0);
		mData = 0;
	}
#else
	CCP_FREE( mData );
	mData = nullptr;
#endif

	return true;
}


//--------------------------------------------------------------------
// ICacheable::GetMemoryUsage
//--------------------------------------------------------------------
bool ResFile::IsMemoryUsageKnown()
{
	return true;
}

size_t ResFile::GetMemoryUsage()
{
	//asynch loading not supported.  If data isn't mapped yet, report
	//memory as 0
	if (mData)
		return mSize;
	else
		return 0;
}


#if RESFILE_PRELOADING_SUPPORTED

struct ThreadArguments
{
	ResFile* file;
	char* data;
	ssize_t size;
	int error;
	bool done;
};


static DWORD WINAPI ThreadThunker(void *p) 
{
	CCP_STATS_ZONE( "ResFile/ThreadThunker" );

	ThreadArguments* args = (ThreadArguments*)p;

	args->size = args->file->GetSize();
	args->data = new char[args->size];
	if (!args->data)
	{
		args->error = E_OUTOFMEMORY;
		args->done = true;
		return -1;
	}

	// TODO: http://msdn2.microsoft.com/en-us/library/aa366801.aspx
	// Reading and Writing From a File View
	
	if (args->file->Read(args->data, args->size) != args->size)
	{
		delete[] args->data;
		args->error = E_OUTOFMEMORY;
		args->done = true;
		return -1;
	}

	args->done = true;
	BeOS->NextScheduledEvent(0);
	return 0;
}

#endif

#endif
