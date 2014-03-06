/* 
	*************************************************************************

	IBluePersist.h

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Almost a final version of Blue's serialization mechanism.  What's
		missing here is a more version control friendly functionality.  The
		final version will incorporate things "borrowed" from Borland VCL.


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _IBLUEPERSISTS_H_
#define _IBLUEPERSISTS_H_

//////////////////////////////////////////////////////////////////////
//
// Blue stream interface
//
//////////////////////////////////////////////////////////////////////
enum BLUESEEK
{
	BS_BEGIN		= 0,
	BS_CURRENT		= 1,
	BS_END			= 2,
};

BLUE_INTERFACE(IBlueStream) : public IRoot
{
	// Reads from the stream into a buffer.
	// Returns the number of bytes read.
	virtual ssize_t Read(
		void* dest,
		ssize_t count
		) = 0;
	
	// Writes from a buffer into the stream.
	// Returns the number of bytes written.
	virtual ssize_t Write(
		const void* source,
		size_t count
		) = 0;

	virtual ssize_t Seek(
		ssize_t distance,
		BLUESEEK method
		) = 0;

	virtual ssize_t GetPosition(
		) = 0;

	virtual ssize_t GetSize(
		) = 0;

	virtual bool LockData(
		void** data,
		size_t size
		) = 0;

	virtual bool UnlockData(
		) = 0;
};



BLUE_INTERFACE( IBlueMemStream ) : public IBlueStream
{
	//Set the buffer which the stream uses
	//if buf is zero, it will allocate a buffer and own it
	//if it is non-zero, it merely references an existing buffer.
	virtual bool SetBuffer(void *buf, size_t size) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// Blue ResFile interface
//
//////////////////////////////////////////////////////////////////////

BLUE_INTERFACE(IResFile) : public IBlueStream
{
	virtual bool Open( //still used by some
		const char* filename,
		bool readOnly
		) = 0;

	virtual bool Close(
		) = 0;

	virtual bool OpenW(
		const wchar_t *filename,
		bool readOnly
		) = 0;

	virtual bool CreateW(
		const wchar_t* filename
		) = 0;

	virtual bool FileExistsW(
		const wchar_t* filename
		) = 0;

	virtual bool Preload(bool &started) = 0;
	virtual bool PreloadInProgress() = 0;
};


#endif
