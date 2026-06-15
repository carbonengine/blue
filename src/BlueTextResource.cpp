// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueTextResource.h"
#include "IBlueResMan.h"

IBlueResource* CreateTextResource( const wchar_t* name )
{
	BlueTextResourcePtr p;
	p.CreateInstance();
	return p.Detach();
}

BLUE_REGISTER_RESOURCE_EXTENSION( L"txt", CreateTextResource );


BlueTextResource::BlueTextResource( IRoot* lockobj /*= NULL */ )
{

}

bool BlueTextResource::IsMemoryUsageKnown()
{
	return !IsLoading();
}

size_t BlueTextResource::GetMemoryUsage()
{
	return m_text.size();
}

BlueAsyncRes::LoadingResult BlueTextResource::DoLoad()
{
	auto size = m_dataStream->GetSize();

	CcpMallocBuffer buffer( "BlueTextResource/buffer", size );
	auto bytesRead = m_dataStream->Read( buffer.get(), size );
	if( bytesRead == size )
	{
		m_text = std::string( buffer.get(), size );
		CCP_LOG( "Text read successfully" );
		return LR_SUCCESS;
	}

	CCP_LOGERR( "Failed to read text - bytesRead: %d, size: %d", bytesRead, size );
	return LR_FAILED;
}

bool BlueTextResource::DoPrepare()
{
	return true;
}
