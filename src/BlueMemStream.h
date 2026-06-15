// Copyright © 2000 CCP ehf.

/* 
	*************************************************************************

	BlueMemStream.h

	Project:   Blue

	Description:   

		Implementation of Memory Stream


	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _BLUEMEMSTREAM_H_
#define _BLUEMEMSTREAM_H_

#include "IBluePersist.h"
#include "IMotherLode.h"

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
	bool SetBuffer(void *buf, size_t size) override;

	/////////////////////////////////////////
	// data members

private:
	char* mData;
	size_t mSize;
	size_t mAllocSize;	//size of mData or 0 if it�s not ours
	size_t mPosition;
	bool mLocked;

	bool Grow(size_t reqSize);

public:
	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read(
		void* dest,
		ptrdiff_t count
		) override;
	
	ptrdiff_t Write(
		const void* source,
		size_t count
		) override;

	ptrdiff_t Seek(
		ptrdiff_t distance,
		SeekOrigin method
		) override;

	bool SetSize(
		size_t newsize
		);

	ssize_t CopyFrom(
		IBlueStream* source,
		size_t count
		);

	ptrdiff_t GetPosition(
		) override;

	ptrdiff_t GetSize(
		) override;

	bool LockData(
		void** data,
		size_t size
		) override;

	bool UnlockData(
		) override;


	/////////////////////////////////////////
	// ICacheable interface
	bool IsMemoryUsageKnown() override;
	size_t GetMemoryUsage() override;
};

TYPEDEF_BLUECLASS_WR(MemStream);

	

#endif
