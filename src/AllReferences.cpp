////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Filipp Pavlov
// Created:		February 2024
// Copyright:	CCP 2024
//

#include "StdAfx.h"
#include "AllReferences.h"


AllReferences::SeenObjectSet::SeenObjectSet()
{
	m_buckets.resize( 1024 );
}

bool AllReferences::SeenObjectSet::Visit( IRoot* obj )
{
	auto h = std::hash<IRoot*>()( obj );
	auto& bucket = m_buckets[h % m_buckets.size()];
	auto found = std::find( begin( bucket ), end( bucket ), obj );
	if( found == end( bucket ) )
	{
		bucket.push_back( obj );
		return true;
	}
	return false;
}

void AllReferences::SeenObjectSet::Clear()
{
	for( auto& bucket : m_buckets )
	{
		bucket.clear();
	}
}


void AllReferences::TemporaryData::Clear()
{
	stack.clear();
	seen.Clear();
}


void AllReferences::GeneratedData::Clear()
{
	references.clear();
	byType.clear();
}


bool AllReferences::Update( float sec )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_root )
	{
		return true;
	}

	auto endTime = CcpGetTimestamp() + uint64_t( double( sec ) * CcpGetTimestampFrequency() );

	if( m_temp.stack.empty() )
	{
		m_temp.stack.push_back( m_root );
	}

	while( !m_temp.stack.empty() )
	{
		if( CcpGetTimestamp() > endTime )
		{
			return false;
		}
		IRootPtr obj = m_temp.stack.back();
		m_temp.stack.pop_back();

		if( !m_temp.seen.Visit( obj ) )
		{
			continue;
		}

		auto type = obj->ClassType();
		for( auto other = type; other; other = other->mParentClassInfo )
		{
			for( auto entry = type->mInterfaceTable; entry->mIID; entry++ )
			{
				m_new.byType[*entry->mIID].push_back( obj );
			}
		}

		EnumerateChildren(
			obj,
			[&]( IRoot* child, const Be::VarEntry* entry, ssize_t index ) {
				m_temp.stack.push_back( child );

				Reference reference{};
				reference.parent = obj;
				if( entry )
				{
					reference.type = RouteStep::ATTRIBUTE;
					reference.attr = entry;
				}
				else if( IBlueDictPtr dict = BlueCastPtr( obj ) )
				{
					reference.type = RouteStep::KEY;
					reference.index = index;
				}
				else
				{
					reference.type = RouteStep::INDEX;
					reference.index = index;
				}
				m_new.references[child].push_back( reference );
			} );
	}

	std::swap( m_new, m_current );
	{
		CCP_STATS_ZONE( "Clear" );
		m_new.Clear();
		m_temp.Clear();
	}
	return true;
}

BluePy AllReferences::GetReferences( IRoot* obj )
{
	auto found = m_current.references.find( obj );
	if( found == end( m_current.references ) )
	{
		return BluePy( PyList_New( 0 ) );
	}

	auto result = PyList_New( Py_ssize_t( found->second.size() ) );
	ssize_t i = 0;
	for( auto& rec : found->second )
	{
		PyObject* element;
		switch( rec.type )
		{
		case RouteStep::ATTRIBUTE:
			element = Py_BuildValue( "(Nis)", BlueWrapObjectForPython( rec.parent ), 0, rec.attr->mName );
			break;
		case RouteStep::KEY:
			element = Py_BuildValue( "(Nis)", BlueWrapObjectForPython( rec.parent ), 1, IBlueDictPtr( BlueCastPtr( rec.parent ) )->GetKey( rec.index ) );
			break;
		default:
			element = Py_BuildValue( "(Nii)", BlueWrapObjectForPython( rec.parent ), 1, rec.index );
		}
		PyList_SetItem( result, i++, element );
	}
	return BluePy( result );
}

std::vector<IRootPtr> AllReferences::FindInterface( IRoot* obj, const char* iidName )
{
	std::vector<IRootPtr> result;
	auto found = m_current.byType.find( Be::IID( iidName ) );

	if( found != end( m_current.byType ) )
	{
		if( obj == m_root )
		{
			result.insert( end( result ), begin( found->second ), end( found->second ) );
		}
		else
		{
			result.reserve( found->second.size() );
			std::unordered_map<IRoot*, bool> seen;
			for( auto& child : found->second )
			{
				if( HasRoute( obj, child, seen ) )
				{
					result.push_back( child );
				}
			}
		}
	}
	return result;
}

void AllReferences::SetRoot( IRoot* root )
{
	if( root == m_root )
	{
		return;
	}
	m_root = root;
	m_new.Clear();
	m_current.Clear();
	m_temp.Clear();
}

IRootPtr AllReferences::GetRoot() const
{
	return m_root;
}

bool AllReferences::HasRoute( IRoot* from, IRoot* to, std::unordered_map<IRoot*, bool>& hasRoute ) const
{
	auto seen = hasRoute.find( to );
	if( seen != end( hasRoute ) )
	{
		return seen->second;
	}

	auto found = m_current.references.find( to );
	if( found == end( m_current.references ) )
	{
		hasRoute[to] = false;
		return false;
	}
	for( auto& ref : found->second )
	{
		if( HasRoute( from, ref.parent, hasRoute ) )
		{
			hasRoute[to] = true;
			return true;
		}
	}
	hasRoute[to] = false;
	return false;
}