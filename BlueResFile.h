/* 
	*************************************************************************

	BlueResFile.h

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Implementation of ResFile


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUERESFILE_H_
#define _BLUERESFILE_H_

#if USE_RESFILE_2

#include "BlueResFile2.h"

#else

#include "include/IBluePersist.h"
#include "include/IBlueOS.h"
#include "include/IBluePython.h"
#include "include/IMotherLode.h"


BLUE_DECLARE( ResFile );

#ifdef _WIN32
	#define RESFILE_PRELOADING_SUPPORTED 1
#else
	#define RESFILE_PRELOADING_SUPPORTED 0
#endif

BLUE_CLASS( ResFile ) : 
	public IResFile,
	public ICacheable
{
public:
	EXPOSE_TO_BLUE();

	ResFile();
	~ResFile();
	
private: 

#ifdef _WIN32
	HANDLE mFile;		//file if we own it (null if this is from stuffer)
	HANDLE mMapping;	//file mapping handle
#else
	FILE* mFile;
#endif


	std::wstring mFilename;
	
	void* mData;		//locked date
	size_t mSize;		//size of data
	size_t mPosition;

	int mStuffHandle;	//used with stuff files;
	int mLockCount;		//how many lockings?
	int mSoftspace;		//for python only
	bool mReadOnly;
	bool mAscii;

	bool IsOpen() const;
	bool OpenWImpl(bool &error, const wchar_t *fn, bool readOnly, bool exists, bool silent);
	bool OpenWStuff(const wchar_t *fn, bool exists);
	bool OpenWFS(bool &error, const wchar_t *fn, bool exists, bool readOnly, bool silent);
	
#if RESFILE_PRELOADING_SUPPORTED
	//preloading
	static DWORD WINAPI PreloadThreadThunk(void *arg);
	DWORD PreloadThread();
	void PreloadWait();
	//Starts out as 0.  is 1 while preloading, -1 when done
	volatile uint32_t mPreloadCount;
#endif

#if BLUE_WITH_PYTHON
	PyObject* PyOpen( PyObject* args );
	PyObject* Pyopen( PyObject* args );
	PyObject* PyOpenAlways( PyObject* args );
	PyObject* PyCreate( PyObject* args );
	PyObject* PyClose( PyObject* args );
	PyObject* PyGetFileInfo( PyObject* args );
	PyObject* PyFileExists( PyObject* args );
	PyObject* Pyread( PyObject* args );
	PyObject* Pyclose( PyObject* args );
	PyObject* Py__enter__( PyObject* args );
	PyObject* Py__exit__( PyObject* args );

	PyObject* PySeekStandard( PyObject* args );
#endif


public:

	/////////////////////////////////////////
	// IResFile interface
	bool Open( const char* filename, bool readOnly );
	bool Close();
	bool OpenW(	const wchar_t* filename, bool readOnly );
	bool CreateW( const wchar_t* filename );
	bool FileExistsW( const wchar_t* filename );
	bool Preload(bool &);
	bool PreloadInProgress();

	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read( void* dest, ptrdiff_t count	);
	ptrdiff_t Write( const void* source, size_t count	);
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method );
	ptrdiff_t GetPosition();
	ptrdiff_t GetSize();
	bool LockData( void** data, size_t size	);
	bool UnlockData();

	/////////////////////////////////////////
	// ICacheable interface
	bool IsMemoryUsageKnown();
	size_t GetMemoryUsage();

};

TYPEDEF_BLUECLASS_WR(ResFile);

#endif
#endif
