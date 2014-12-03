////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

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
	CCP_ASSERT( !m_contents );
}

void BackgroundReader::Perform()
{
	if( !m_contents.CreateInstance() )
	{
		m_result = Be::Result<std::string>( "Couldn't create MemStream object" );
		return;
	}

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
