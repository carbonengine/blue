#pragma once

#include "include/Blue.h"
#include <string>
#include <set>
#include "include/IBluePersist.h"

#ifndef _WIN32
// TODO: temporary hack while porting
typedef uint32_t HANDLE;
typedef uint32_t DWORD;
typedef int32_t LONG;
#define WINAPI
#define CloseHandle(h)
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#endif


// Wrapper class for a Windows HANDLE. Ensures that the
// HANDLE is closed.
class Handle
{
public:
	Handle() :	handle(0) {}
	Handle(const HANDLE &h) :  handle(h) {}
	~Handle() {Close();}

	Handle &operator=(const HANDLE h)
	{
		Close();
		handle = h;
		return *this;
	}
	void Close()
	{
		if (Valid())
			CloseHandle(handle);
		handle = 0;
	}

	operator HANDLE() const
	{
		return handle;
	}

	bool Invalid() const {return handle == INVALID_HANDLE_VALUE;} // check only for invalid
	bool operator ! () const {return !handle;}	//check for null
	bool Valid() const {return handle!=0  && !Invalid();} //checks for null too

private:
	HANDLE handle;
};



class EmbedFs
{
public:
	struct HashValue;

	EmbedFs( const char* embedfile );
	EmbedFs( const wchar_t* embedfile );
	~EmbedFs();
	
	bool GetStream( size_t offset, size_t size, IBlueStream** stream );

	unsigned NumFiles() const;
	bool LoadHashValues(HashValue* hv, unsigned stuffFileNo);
	bool MapFile();

	void *GetFileData(size_t fileoffs, size_t filesize);
	void ReleaseFileData(void *p);
	static std::wstring ToLower(const wchar_t *s);
	static std::string ToLower(const char *s);

	struct HashValue
	{
		HashValue() : crc(0) {};

		HashValue(EmbedFs &efs, const char *n, size_t _offs, size_t _size, int _idx);
		HashValue(EmbedFs &efs, const wchar_t *n, size_t _offs, size_t _size, int _idx);
	
		bool operator < (const HashValue &o) const {
			return crc < o.crc;
		}

		unsigned long crc;
		std::string fname; //The actual name
		int stuffFile;
		size_t size;
		size_t offs;
	};

private:
	int Init();
	void *GetFileDataRaw(size_t fileoffs, size_t filesize);
	static bool Read(const void *map, size_t &ptr, void *dest, size_t size);
	static void Normalize(std::string &s);
	
private:
	static DWORD sAllocGran;//system allocation granularity
	
	std::wstring mEfsfile;  //filename, owned by us
	Handle mFile;
	Handle mMapping;
	
	size_t mOrgSize;  //size of ignored data at the head of file
	size_t mDataStart;
	size_t mDataEnd;	 //byte after end of file data
	size_t mDataSize; //Size of data
	unsigned mNumfiles;
	
	DWORD mOffset;	 //the distance from mMapped to mDataStart
};