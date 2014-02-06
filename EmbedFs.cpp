// EmbedFs.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"

#if STUFFER_ENABLED

#include "EmbedFs.h"

#include "zlib.h"
#include <string>
#include <cctype>
#include <cwctype>

#include "include/Blue.h"
#include "include/IBlueOS.h"

#if CCP_STACKLESS
#include <CcpUtils/PyCpp.h>
using Ccp::PyAllowThreads;
#endif

#include "BlueStuffFileStream.h"

static CcpLogChannel_t s_myChannel = CCP_LOG_DEFINE_CHANNEL( "File" );

static const char MARKER[] = "EmbedFs 1.0";

EmbedFs::HashValue::HashValue( EmbedFs &efs, const char *n, size_t _offs, size_t _size, int _idx ) : 
	offs( _offs ), size( _size ), stuffFile( _idx )
{
	fname = ToLower( n );
	crc = crc32( 1, (const unsigned char*)fname.c_str(), (int)fname.size() );
}
EmbedFs::HashValue::HashValue( EmbedFs &efs, const wchar_t *n, size_t _offs, size_t _size, int _idx ) : 
	offs( _offs ), size( _size ), stuffFile( _idx )
{
	fname = CW2A( ToLower(n).c_str() );
	crc = crc32( 1, (const unsigned char*)fname.c_str(), (int)fname.size() );
}

std::string EmbedFs::ToLower(const char *s)
{
	std::string res;
	for(const char *cp = s; cp && *cp; cp++) {
		int c = *cp;
		if (__isascii(c) && isupper(c))
			c = _tolower(c);
		res.push_back(c);
	}
	return res;
}


std::wstring EmbedFs::ToLower(const wchar_t *s)
{
	std::wstring res;
	for( const wchar_t *cp = s; cp && *cp; cp++ )
	{
		wint_t c = *cp;
		if( std::iswupper( c ) )
		{
			c = std::towlower( c );
		}
		res.push_back( c );
	}
	return res;
}

// Monitoring seek time
static BeTimer gSeekCounter;
static int gNumAccesses = 0;
static Be::Time gTotalTime = 0;

DWORD EmbedFs::sAllocGran = 0;
EmbedFs::EmbedFs( const wchar_t* embedfile )
{
	mOrgSize = mDataStart = mDataEnd = mDataSize = 0;
	mNumfiles = 0;
	
	mEfsfile = embedfile;
	if( !sAllocGran ) 
	{
#ifdef _WIN32
		SYSTEM_INFO si;
		GetSystemInfo( &si );
		sAllocGran = si.dwAllocationGranularity;
#endif
	}
	Init();
}

EmbedFs::EmbedFs( const char* embedfile )
{
	mOrgSize = mDataStart = mDataEnd = mDataSize = 0;
	mNumfiles = 0;

	mEfsfile = CA2W(embedfile);
	if (!sAllocGran) {
#ifdef _WIN32
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		sAllocGran = si.dwAllocationGranularity;
#endif
	}
	Init();
}

EmbedFs::~EmbedFs()
{
	//for cleanliness, close here.
	mMapping.Close();
	mFile.Close();
}


void *EmbedFs::GetFileData(size_t fileoffs, size_t filesize)
{
	return GetFileDataRaw(fileoffs+mDataStart, filesize);
}
	
void *EmbedFs::GetFileDataRaw(size_t fileoffs, size_t filesize)
{
	if (fileoffs+filesize > mDataEnd)
	{
		CCP_LOGERR_CH( s_myChannel, "GetFileData, failed getting %d bytes at %d in %S which has %d bytes", filesize, fileoffs, mEfsfile.c_str(), mDataEnd );
		return nullptr;
	}

	if (!mMapping && !MapFile())
	{
		return nullptr;
	}

	size_t start = fileoffs;
	// We need to map a certain granularity.  It's okay if we overlap with another file.
	// move start position back to closest offset
	size_t offset = start % sAllocGran;
	start -= offset;
	size_t len = filesize + offset;
	
	LARGE_INTEGER li;
	li.QuadPart = start;
	void * res = MapViewOfFile(mMapping, FILE_MAP_READ, li.HighPart, li.LowPart, len);
	if (!res)
	{
		CCP_LOGERR_CH( s_myChannel, "Couldn't map view of file %S", mEfsfile.c_str() );
		return nullptr;
	}
	return (void*) ((char*)res + offset);
}


void EmbedFs::ReleaseFileData(void *p)
{
	// We have to assume that the addresses returned by MapViewOfFile were similarly
	// aligned as the offsets within files.  Retrieve the aligned address
	void *addr = (void*)((UINT_PTR)p - ((UINT_PTR)p % sAllocGran));
	if (!UnmapViewOfFile(addr))
	{
		CCP_LOGERR_CH( s_myChannel, "Failed to unmap view of file" );
	}
}

unsigned EmbedFs::NumFiles() const
{
	return mNumfiles;
}


#define READ(__buff, __buffsize)	\
	if (!ReadFile(mFile, __buff, __buffsize, &got, NULL)) return false;

int EmbedFs::Init()
{
	mNumfiles = 0;
	{
#if CCP_STACKLESS
		PyAllowThreads _allow(true);
#endif
		mFile = CreateFileW( mEfsfile.c_str(),
				GENERIC_READ,FILE_SHARE_READ,NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
				NULL);
	}
	
	if (mFile.Invalid()) {
		CCP_LOGERR_CH( s_myChannel, "Failed to open EFS file %S", mEfsfile.c_str() );
		return 0;
	}

	// File layout:    [orgsize bytes of junk] indexdata, filedata, orgsize, marker

	DWORD fPtr = SetFilePointer(mFile, - long(sizeof MARKER), NULL, FILE_END);
	if (fPtr == INVALID_SET_FILE_POINTER) {
		CCP_LOGERR_CH( s_myChannel, "Failed to set filepointer on EFS file %S", mEfsfile.c_str() );
		return 0;
	}
	DWORD got;
	char marker[sizeof MARKER];
	READ(marker, sizeof marker);
	if (strcmp(marker, MARKER) != 0) {
		CCP_LOGERR_CH( s_myChannel, "Invalid marker in EFS file %S", mEfsfile.c_str() );
		return 0;
	}

	DWORD orgsize;
	fPtr = SetFilePointer(mFile, - long(sizeof MARKER + sizeof orgsize), NULL, FILE_END);
	if (fPtr == INVALID_SET_FILE_POINTER) {
		CCP_LOGERR_CH( s_myChannel, "Failed to set filepointer on EFS file %S", mEfsfile );
		return 0;
	}
	mDataEnd = fPtr; //start of 'orgsize' value is end of file data
	READ(&orgsize, sizeof orgsize);
	mOrgSize = orgsize;

	//sanity check on orgsize
	if (mOrgSize+sizeof(unsigned) > mDataEnd)
		CCP_LOGERR_CH( s_myChannel, "Invalid stuff file, orgsize too large at %d bytes", mOrgSize );

	//now, read number of files
	fPtr = SetFilePointer(mFile, orgsize, NULL, FILE_BEGIN);
	if (fPtr == INVALID_SET_FILE_POINTER) {
		CCP_LOGERR_CH( s_myChannel, "Failed to set filepointer on EFS file %S", mEfsfile.c_str() );
		return 0;
	}	
	unsigned numFiles;
	READ(&numFiles, sizeof numFiles);
	mNumfiles = numFiles;
	return mNumfiles;
}


bool EmbedFs::Read(const void *map, size_t &ptr, void *dest, size_t len)
{
	__try
	{
		memcpy(dest, (const char*)map + ptr, len);
		ptr += len;
	} 
	__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? 
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		SetLastError(ERROR_READ_FAULT);
		CCP_LOGERR_CH( s_myChannel, "Couldn't Read from mapped file" );
		return false;
	}
	return true;
}
	

//Read in the file data and create the file mapping
bool EmbedFs::LoadHashValues(HashValue* hv, unsigned stuffFileNo)
{
	const wchar_t* stuffName = wcsrchr(mEfsfile.c_str(), L'/');
	if (stuffName == NULL)
	{
		stuffName = mEfsfile.c_str();
	}
	else
	{
		stuffName++;
	}
	CCP_LOG_CH( s_myChannel, "Loading index for stuff file %S", stuffName );

	if (!MapFile())
		return false;

	LARGE_INTEGER li;
	li.QuadPart = mOrgSize;
	void *map = GetFileDataRaw(mOrgSize, mDataEnd-mOrgSize);
	if (!map)
		return false;

	size_t ptr = sizeof(unsigned);	
	HashValue *hv_org = hv;
	size_t offset = 0;
	unsigned i;
	for (i = 0; i < mNumfiles; i++)
	{
		unsigned filesize, namelen;
		if (!Read(map, ptr, &filesize, sizeof filesize) ||
			!Read(map, ptr, &namelen, sizeof namelen)) {
			ReleaseFileData(map);
			return false;
		}
		std::vector<char> filename(namelen+1);
		if (!Read(map, ptr, &filename[0], namelen + 1)) {
			ReleaseFileData(map);
			return false;
		}
		filename[namelen] = 0;
		std::string fn(&filename[0]);
		Normalize(fn);
		*hv = HashValue(*this, fn.c_str(), offset, filesize, stuffFileNo);
		offset += hv->size;
		hv++;
	}
	ReleaseFileData(map);

	mDataStart = ptr+mOrgSize;
	mDataSize = mDataEnd-mDataStart;

	//sanity check the offsets
	bool ok = true;
	hv = hv_org;
	for (i = 0; i < mNumfiles; i++)
	{
		if (hv[i].offs+hv[i].size > mDataSize) {
			CCP_LOGERR_CH( s_myChannel, "File %s in stuff file %S invalid index entry with start=%d, size=%d", hv[i].fname.c_str(), stuffName, hv[i].offs, hv[i].size );
			ok = false;
		}
	}

	return ok;
}


void EmbedFs::Normalize(std::string &s)
{
	//cut off turn initial res
	if (s.find("res\\") == 0)
		s = s.substr(3);
	size_t i;
	for(i=0; i<s.size(); ++i) {
		if (s[i] == '\\')
			s[i] = '/';
		if (i && s[i] == '/' && s[i-1] == '/') {
			s.erase(i);
			--i;
		}
	}
}


bool EmbedFs::MapFile()
{
	// now, we map the file, if we are so inclined
	if (mMapping)
		return true;
	LARGE_INTEGER li;
	li.QuadPart = mDataEnd;
	mMapping = CreateFileMapping( mFile, NULL, PAGE_READONLY,
		li.HighPart, li.LowPart,  //map only to the end of data
		NULL);
	if (!mMapping) {
		CCP_LOGERR_CH( s_myChannel, "Failed to create file mapping for %S", mEfsfile.c_str() );
		return false;
	}
	return true;
}

bool EmbedFs::GetStream( size_t offset, size_t size, IBlueStream** stream )
{
	BlueStuffFileStream* ss = NULL;

	if( BeClasses->CreateInstance( GetBlueStuffFileStreamClsid(), GetBlueStuffFileStreamIID(), (void**)&ss ) )
	{
		HANDLE handle = CreateFileW( 
			mEfsfile.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
			NULL
			);

		ss->SetHandle( handle, offset + mDataStart, size );

		*stream = ss;
		return true;
	}

	return false;
}

#endif
