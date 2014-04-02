////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#if STUFFER_ENABLED

#include "BlueStuffFileStream.h"
#include "include\IBlueOS.h"
#include "CcpUtils\PyCpp.h"

BlueStuffFileStream::BlueStuffFileStream() :
	m_fileHandle( INVALID_HANDLE_VALUE ),
	m_offset( 0 ),
	m_dataSize( 0 ),
	m_data( NULL )
{
}

BlueStuffFileStream::~BlueStuffFileStream()
{
	if( m_data )
	{
		ClearLockedData();
	}

	if( ( m_fileHandle != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_fileHandle );
	}
}

void BlueStuffFileStream::SetHandle( HANDLE handle, size_t offset, size_t size )
{
	CCP_ASSERT( m_fileHandle == INVALID_HANDLE_VALUE );

	m_fileHandle = handle;
	m_offset = offset;
	m_dataSize = size;

	SetFilePointer( m_fileHandle, (DWORD)m_offset, NULL, FILE_BEGIN );
}

ptrdiff_t BlueStuffFileStream::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_fileHandle == INVALID_HANDLE_VALUE )
	{
		return -1;
	}

	Ccp::PyAllowThreads allowThreads( true );

	ssize_t curPos = GetPosition();
	if( curPos == -1 )
	{
		return 0;
	}

	if( curPos + count > (ssize_t)m_dataSize )
	{
		count = m_dataSize - curPos;
	}

	DWORD read;
	BOOL ok = ReadFile( m_fileHandle, dest, (DWORD)count, &read, NULL );

	if( !ok )
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Read");
		return -1;
	}

	return read;
}

ptrdiff_t BlueStuffFileStream::Write( const void* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueStuffFileStream::Seek( ptrdiff_t distance, SeekOrigin method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	DWORD oldPos = SetFilePointer( m_fileHandle, 0, NULL, FILE_CURRENT );

	if( oldPos == INVALID_SET_FILE_POINTER )
	{
		return INVALID_SET_FILE_POINTER;
	}

	DWORD newPos;

	switch( method )
	{
		case SO_BEGIN:
			newPos = (DWORD)m_offset + (DWORD)distance;
			break;
		
		case SO_CURRENT:
			newPos = oldPos + (DWORD)distance;
			break;

		case SO_END:
			newPos = (DWORD)m_offset + (DWORD)m_dataSize - (DWORD)distance;
			break;
	}

	DWORD ret = SetFilePointer( m_fileHandle, newPos, NULL, FILE_BEGIN );

	if( ret == INVALID_SET_FILE_POINTER )
	{
		return INVALID_SET_FILE_POINTER;
	}

	ret -= (DWORD)m_offset;
	if( ret >= m_dataSize )
	{
		// Invalid position - before the start or past the end of the file
		SetFilePointer( m_fileHandle, oldPos, NULL, FILE_BEGIN );
		return INVALID_SET_FILE_POINTER;
	}

	return ret;
}

bool BlueStuffFileStream::SetSize( size_t newsize )
{
	return false;
}

ssize_t BlueStuffFileStream::CopyFrom( IBlueStream* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueStuffFileStream::GetPosition()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_fileHandle == INVALID_HANDLE_VALUE )
	{
		return -1;
	}

	DWORD ret = SetFilePointer( m_fileHandle, 0, NULL, FILE_CURRENT );

	ret -= (DWORD)m_offset;

	if( ret >= m_dataSize )
	{
		ret = -1;
	}

	return ret;
}

ptrdiff_t BlueStuffFileStream::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return m_dataSize;
}

bool BlueStuffFileStream::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_fileHandle == INVALID_HANDLE_VALUE )
	{
		// No file handle, can't do anything
		return false;
	}

	if( m_data )
	{
		// Already locked
		return false;
	}

	m_data = CCP_MALLOC( "BlueStuffFileStream/m_data", m_dataSize );
	if( !m_data )
	{
		// Couldn't get memory
		return false;
	}

	if( Seek( 0, SO_BEGIN ) < 0 )
	{
		ClearLockedData();
		return false;
	}

	if( Read( m_data, m_dataSize ) != m_dataSize )
	{
		ClearLockedData();
		return false;
	}

	*data = m_data;
	return true;
}

bool BlueStuffFileStream::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_data )
	{
		// Not locked - nothing to do
		return false;
	}

	ClearLockedData();
	return true;
}

void BlueStuffFileStream::ClearLockedData()
{
	m_dataSize = 0;
	CCP_FREE( m_data );
	m_data = NULL;
}

#endif
