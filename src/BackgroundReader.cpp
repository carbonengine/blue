// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BackgroundReader.h"
#include "BlueMemStream.h"
#include "BlueResFile.h"

BackgroundReader::BackgroundReader( const std::wstring& filename ) : 
m_filename( filename )
{

}

BackgroundReader::~BackgroundReader()
{

}

void BackgroundReader::Perform()
{
	ResFilePtr resFile;
	if( !resFile.CreateInstance() )
	{
		m_result = Be::Result<std::string>( "Couldn't create ResFile object" );
		return;
	}

	if (!resFile->OpenW( m_filename.c_str(), true ) )
	{
		m_result = Be::Result<std::string>( "Couldn't open file" );
		return;
	}

	void* data;
	size_t dataSize = resFile->GetSize();
	if( !resFile->LockData( &data, dataSize ) )
	{
		m_result = Be::Result<std::string>( "Couldn't read data" );
		return;
	}

	if( !m_contents.CreateInstance() )
	{
		resFile->UnlockData();
		m_result = Be::Result<std::string>( "Couldn't create MemStream object" );
		return;
	}
	m_contents->Write( data, dataSize );
	m_contents->Seek( 0, ICcpStream::SO_BEGIN );

	resFile->UnlockData();
}

Be::Result<std::string> BackgroundReader::GetResult()
{
	return m_result;
}

void BackgroundReader::TakeContents( IBlueStream** contents )
{
	*contents = m_contents.Detach();
}
