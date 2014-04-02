/* 
	*************************************************************************

	BlueMemStream.h

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Implementation of Memory Stream


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUEMEMSTREAM_H_
#define _BLUEMEMSTREAM_H_

#include "include/IBluePersist.h"
#include "include/IMotherLode.h"

BLUE_DECLARE( MemStream );

BLUE_CLASS( MemStream ): 
	public IBlueMemStream,
	public ICacheable
{
public:
	EXPOSE_TO_BLUE();

	/////////////////////////////////////////
	// Public member functions

	MemStream();
	~MemStream();

	/////////////////////////////////////////
	// IBlueMemStream interface
	bool SetBuffer(void *buf, size_t size);

	/////////////////////////////////////////
	// data members

private:
	char* mData;
	size_t mSize;
	size_t mAllocSize;	//size of mData or 0 if it´s not ours
	size_t mPosition;
	bool mLocked;

	bool Grow(size_t reqSize);

public:
	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read(
		void* dest,
		ptrdiff_t count
		);
	
	ptrdiff_t Write(
		const void* source,
		size_t count
		);

	ptrdiff_t Seek(
		ptrdiff_t distance,
		SeekOrigin method
		);

	bool SetSize(
		size_t newsize
		);

	ssize_t CopyFrom(
		IBlueStream* source,
		size_t count
		);

	ptrdiff_t GetPosition(
		);

	ptrdiff_t GetSize(
		);

	bool LockData(
		void** data,
		size_t size
		);

	bool UnlockData(
		);


	/////////////////////////////////////////
	// ICacheable interface
	bool IsMemoryUsageKnown();
	size_t GetMemoryUsage();
};

TYPEDEF_BLUECLASS_WR(MemStream);

	

#endif
