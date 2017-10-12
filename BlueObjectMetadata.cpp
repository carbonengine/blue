#include "StdAfx.h"
#include "BlueObjectMetadata.h"

IBlueObjectMetadata* BeObjectMetadata = nullptr;

BlueObjectMetadata::DataTable::DataTable()
	:mapping( "BlueObjectMetadata::DataTable::mapping" )
{
};

BlueObjectMetadata::BlueObjectMetadata( IRoot* lockobj )
	:m_metadata( "BlueObjectMetadata::m_metadata" )
{
}

BlueObjectMetadata::~BlueObjectMetadata()
{
	for( auto it = m_metadata.begin(); it != m_metadata.end(); ++it )
	{
		it->first->WeakRefUnregister( this );
		CCP_DELETE it->second;
	}
}

const BlueObjectMetadata::Metadata* BlueObjectMetadata::GetMetadata( IWeakObject* owner ) const
{
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return nullptr;
	}
	return &found->second->mapping;
}

void BlueObjectMetadata::Set( IWeakObject* owner, const char* key, const char* value )
{
	if( !owner )
	{
		return;
	}
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		auto table = CCP_NEW( "BlueObjectMetadata::DataTable" ) DataTable;
		found = m_metadata.insert( std::make_pair( owner, table ) ).first;
		owner->WeakRefRegister( this );
	}
	found->second->mapping[key] = value;
}

const char* BlueObjectMetadata::Get( IWeakObject* owner, const char* key, const char* defaultValue ) const
{
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return defaultValue;
	}
	auto keyFound = found->second->mapping.find( key );
	if( keyFound == found->second->mapping.end() )
	{
		return defaultValue;
	}
	return keyFound->second.c_str();
}

BlueStdResult BlueObjectMetadata::Delete( IWeakObject* owner, const char* key )
{
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "object not found in the database" );
	}
	if( !found->second->mapping.erase( key ) )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "key not found in the database" );
	}
	if( found->second->mapping.empty() )
	{
		found->first->WeakRefUnregister( this );
		CCP_DELETE found->second;
		m_metadata.erase( found );
	}
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

BlueStdResult BlueObjectMetadata::DeleteObject( IWeakObject* owner )
{
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "object not found in the database" );
	}
	found->first->WeakRefUnregister( this );
	CCP_DELETE found->second;
	m_metadata.erase( found );
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

BlueStdResult BlueObjectMetadata::Index( IWeakObject* owner, const char* key, const char*& value ) const
{
	value = nullptr;
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "object not found in the database" );
	}
	auto keyFound = found->second->mapping.find( key );
	if( keyFound == found->second->mapping.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "key not found in the database" );
	}
	value = keyFound->second.c_str();
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

BlueStdResult BlueObjectMetadata::GetKeys( IWeakObject* owner, std::vector<std::string>& keys ) const
{
	keys.clear();
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "object not found in the database" );
	}
	auto& mapping = found->second->mapping;
	keys.reserve( mapping.size() );
	for( auto it = mapping.begin(); it != mapping.end(); ++it )
	{
		keys.push_back( it->first );
	}
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

BlueStdResult BlueObjectMetadata::GetItems( IWeakObject* owner, std::map<std::string, std::string>& items ) const
{
	items.clear();
	auto found = m_metadata.find( owner );
	if( found == m_metadata.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_KEY_ERROR, "object not found in the database" );
	}
	auto& mapping = found->second->mapping;
	items.insert( mapping.begin(), mapping.end() );
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

void BlueObjectMetadata::WeakRefNotify( IWeakObject* weak )
{
	DeleteObject( weak );
}
