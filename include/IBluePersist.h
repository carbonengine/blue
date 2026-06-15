// Copyright © 2000 CCP ehf.

/* 
	*************************************************************************

	IBluePersist.h

	Project:   Blue

	Description:   

		Almost a final version of Blue's serialization mechanism.  What's
		missing here is a more version control friendly functionality.  The
		final version will incorporate things "borrowed" from Borland VCL.


	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _IBLUEPERSISTS_H_
#define _IBLUEPERSISTS_H_

//////////////////////////////////////////////////////////////////////
//
// Blue stream interface
//
//////////////////////////////////////////////////////////////////////

BLUE_INTERFACE(IBlueStream) : public IRoot, public ICcpStream
{
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
