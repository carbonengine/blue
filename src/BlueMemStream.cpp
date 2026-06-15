// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueMemStream.h"

#include "IBlueOS.h"


//////////////////////////////////////////////////////////////////////
//
// Public member functions
//
//////////////////////////////////////////////////////////////////////

#include <set>
#include <algorithm>
using namespace std;

//--------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------

MemStream::MemStream()
{
	//TRACE("MemStream::MemStream()\n");

	mData = 0;
	mAllocSize = 0;
	mSize = 0;
	mPosition = 0;
	mLocked = false;
}


MemStream::~MemStream()
{
	if (mAllocSize > 0)
		CCP_FREE(mData);
}


//--------------------------------------------------------------------
// Set a buffer to point to particular memory
//--------------------------------------------------------------------
bool MemStream::SetBuffer(void *ptr, size_t size)
{
	bool own;
	if (!ptr) 
	{
		ptr = (void*)CCP_MALLOC("MemStream/mData", size);
		if (!ptr) 
		{
			CCP_LOGERR( "Couldn't alloc %d bytes.", size );
			return false;
		}
		own = true;
	} 
	else
	{
		own = false;
	}
	if( mAllocSize> 0 )
	{
		CCP_FREE( mData );
	}
	mData = (char*)ptr;
	mSize = size;
	mAllocSize = own ? size : 0;
	mPosition = 0;
	mLocked = false;
	return true;
}

bool MemStream::Grow(size_t reqsize)
{
	if (reqsize > mAllocSize) {
		if (mAllocSize == -1) {
			CCP_LOGERR( "Couldn't grow a provided buffer.");
			return false;
		}
		size_t newAllocSize = mAllocSize * 2;
		if (reqsize > newAllocSize)
			newAllocSize = reqsize;
		char *tmp = (char*)CCP_REALLOC( "MemStream/mData", mData, newAllocSize);
		if (!tmp && newAllocSize > reqsize) {
			//try again with a tighter fit
			newAllocSize = reqsize;
			tmp = (char*)CCP_REALLOC( "MemStream/mData", mData, newAllocSize );
		}
		if (!tmp) {
			CCP_LOGERR( "Couldn't realloc %d bytes.", newAllocSize);
			return false;
		}
		mData = tmp;
		mAllocSize = newAllocSize;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////
//
// IBlueStream interface methods
//
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
// IBlueStream::Read
//--------------------------------------------------------------------
ptrdiff_t MemStream::Read(
	void* dest,
	ptrdiff_t count
	)
{
	if (count < 0 || mPosition + count > mSize)
		count = mSize - mPosition;
	CCP_ASSERT(count >= 0);
	
	memcpy(dest, mData + mPosition, count);
	mPosition += count;
	return count;
}


//--------------------------------------------------------------------
// IBlueStream::Write
//--------------------------------------------------------------------
ptrdiff_t MemStream::Write(
	const void* source,
	size_t count
	)
{
	size_t newPos = mPosition + count;
	if (newPos > mSize && !Grow(newPos))
		return -1;
		
	memcpy(mData + mPosition, source, count);
	mPosition = newPos;
	if (mSize < mPosition)
		mSize = mPosition;

	return count;
}

//--------------------------------------------------------------------
// IBlueStream::Seek
//--------------------------------------------------------------------
ptrdiff_t MemStream::Seek(
	ptrdiff_t distance,
	SeekOrigin method
	)
{
	size_t pos;

	if (method == SO_BEGIN)
		pos = distance;
	else if (method == SO_CURRENT)
		pos = mPosition + distance;
	else
		pos = mSize - distance;

	if (pos > mSize) {
		if (!Grow(pos))
			return -1;
		mSize = pos;
	}

	if (pos > mSize)
	{
		CCP_LOGERR( "Seeking past EOF, size=%d, seekpos=%d.",mSize, pos);
		return -1;
	}

	mPosition = pos;
	return mPosition;
}


//--------------------------------------------------------------------
// IBlueStream::SetSize
//--------------------------------------------------------------------
bool MemStream::SetSize(
	size_t newsize
	)
{
	if (mLocked)
	{
		CCP_LOGERR( "Cannot resize locked stream.");
		return false;
	}

	if (newsize > mSize && !Grow(newsize))
		return false;
	mSize = newsize;

	if (mPosition > mSize)
		mPosition = mSize;

	return true;
}


//--------------------------------------------------------------------
// IBlueStream::CopyFrom
//--------------------------------------------------------------------
ssize_t MemStream::CopyFrom(
	IBlueStream* source,
	size_t count
	)
{
	CCP_LOGERR( "CopyFrom not implemented yet");
	return -1;
}



//--------------------------------------------------------------------
// IBlueStream::GetPosition
//--------------------------------------------------------------------
ptrdiff_t MemStream::GetPosition(
	)
{
	return mPosition;
}


//--------------------------------------------------------------------
// IBlueStream::GetSize
//--------------------------------------------------------------------
ptrdiff_t MemStream::GetSize(
	)
{
	return mSize;
}


//--------------------------------------------------------------------
// IBlueStream::GetSize
//--------------------------------------------------------------------
bool MemStream::LockData(
	void** data,
	size_t size
	)
{
	*data = mData;
	mLocked = true;

	return true;
}


//--------------------------------------------------------------------
// IBlueStream::GetSize
//--------------------------------------------------------------------
bool MemStream::UnlockData(
	)
{
	mLocked = false;
	return true;
}


//--------------------------------------------------------------------
// ICacheable
//--------------------------------------------------------------------
bool MemStream::IsMemoryUsageKnown()
{
	return true;
}

size_t MemStream::GetMemoryUsage()
{
	return mAllocSize;
}
