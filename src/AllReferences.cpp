// Copyright © 2024 CCP ehf.

#include "StdAfx.h"
#include "AllReferences.h"
#include <BlueStatistics.h>


bool AllReferences::Reference::operator==(const Reference& other) const
{
	return parent == other.parent && isAttribute == other.isAttribute && attr == other.attr;
}

bool AllReferences::Reference::operator!=(const Reference& other) const
{
	return !( *this == other );
}


AllReferences::~AllReferences()
{
	SetRoot( nullptr );
}

bool AllReferences::Update( float sec )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_root )
	{
		return true;
	}

	auto startTime = CcpGetTimestamp();
	auto endTime = startTime + uint64_t( double( sec ) * CcpGetTimestampFrequency() );

	if( m_stack.empty() && !m_cleaningReferences )
	{
		m_stack.push_back( m_root );
	}

	while( !m_stack.empty() )
	{
		if( CcpGetTimestamp() > endTime )
		{
			return false;
		}
		IRoot* obj = m_stack.back();
		m_stack.pop_back();
		
		auto type = obj->ClassType();
		for( auto other = type; other; other = other->mParentClassInfo )
		{
			for( auto entry = type->mInterfaceTable; entry->mIID; entry++ )
			{
				m_newByType[entry->mIID->GetHash()].push_back( obj );
			}
		}
		
		EnumerateChildren(
			obj,
			[&]( IRoot* child, const Be::VarEntry* entry, ssize_t index ) {
				child = child->GetRootObject();

				Reference reference{};
				reference.parent = obj;
				reference.generation = m_generation;
				if( entry )
				{
					reference.isAttribute = true;
					reference.attr = entry;
				}
				else
				{
					reference.isAttribute = false;
					reference.index = index;
				}
				auto& refs = m_references[child];

				if( !refs.first.parent )
				{
					child->Lock();
					refs.first = reference;
					refs.generation = m_generation;
					if( child != m_root )
					{
						m_stack.push_back( child );
					}
				}
				else
				{
					if( refs.generation != m_generation )
					{
						refs.generation = m_generation;
						if( child != m_root )
						{
							m_stack.push_back( child );
						}
					}
					if( refs.first != reference )
					{
						auto found = find( begin( refs.rest ), end( refs.rest ), reference );
						if( found == end(refs.rest) )
						{
							refs.rest.push_back( reference );
						}
						else
						{
							found->generation = m_generation;
						}
					}
					else
					{
						refs.first.generation = m_generation;
					}
				}
			} );
	}

	if( !m_cleaningReferences )
	{
		std::swap( m_newByType, m_currentByType );
		for( auto& type : m_newByType )
		{
			type.second.clear();
		}
		m_clearReferencesIt = begin( m_references );
		m_cleaningReferences = true;
	}

	if( !CleanReferences( endTime ) )
	{
		return false;
	}
	m_cleaningReferences = false;
	++m_generation;

	return true;
}

bool AllReferences::CleanReferences( uint64_t endTime )
{
	CCP_STATS_ZONE( __FUNCTION__ );
	uint32_t count = 0;
	while( m_clearReferencesIt != end( m_references ) )
	{
		if( ++count == 100 )
		{
			if( CcpGetTimestamp() > endTime )
			{
				return false;
			}
			count = 0;
		}
		auto& refs = m_clearReferencesIt->second;
		auto generation = m_generation;
		refs.rest.erase( remove_if( begin( refs.rest ), end( refs.rest ), [generation]( auto& x ) { return x.generation != generation; } ), end( refs.rest ) );
		if( refs.first.generation != generation )
		{
			if( refs.rest.empty() )
			{
				m_clearReferencesIt->first->Unlock();
				m_clearReferencesIt = m_references.erase( m_clearReferencesIt );
				continue;
			}
			else
			{
				refs.first = refs.rest.back();
				refs.rest.pop_back();
			}
		}
		++m_clearReferencesIt;
	}
	return true;
}

BluePy AllReferences::GetReferences( IRoot* obj )
{
	auto found = m_references.find( obj );

	auto result = PyList_New( 0 );

	auto AddRef = [result]( auto& rec ) {
		PyObject* element;
		if( rec.isAttribute )
		{
			element = Py_BuildValue( "(Nis)", BlueWrapObjectForPython( rec.parent ), 0, rec.attr->mName );
		}
		else if( IBlueDictPtr dict = BlueCastPtr( rec.parent ) )
		{
			element = Py_BuildValue( "(Nis)", BlueWrapObjectForPython( rec.parent ), 1, dict->GetKey( rec.index ) );
		}
		else
		{
			element = Py_BuildValue( "(Nii)", BlueWrapObjectForPython( rec.parent ), 1, rec.index );
		}
		PyList_Append( result, element );
		Py_XDECREF( element );
	};

	if( found != end( m_references ) && !IsOutdated( found->second ) )
	{
		AddRef( found->second.first );
		for( auto& rec : found->second.rest )
		{
			if( !IsOutdated( rec ) )
			{
				AddRef( rec );
			}
		}
	}
	return BluePy( result );
}

std::vector<IRootPtr> AllReferences::FindInterface( IRoot* obj, const char* iidName )
{
	std::vector<IRootPtr> result;
	auto found = m_currentByType.find( Be::IID( iidName ).GetHash() );

	if( found != end( m_currentByType ) )
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
				auto found = find( begin( result ), end( result ), child );
				if( found == end( result ) && HasRoute( obj, child, seen ) )
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
	if( root )
	{
		root = root->GetRootObject();
	}
	if( root == m_root )
	{
		return;
	}
	m_root = root;
	m_newByType.clear();
	m_currentByType.clear();
	m_stack.clear();
	for( auto& ref : m_references )
	{
		ref.first->Unlock();
	}
	m_references.clear();
	m_generation = 1;
}

IRootPtr AllReferences::GetRoot() const
{
	return m_root;
}

bool AllReferences::HasRoute( IRoot* from, IRoot* to, std::unordered_map<IRoot*, bool>& hasRoute ) const
{
	if( from == to )
	{
		return true;
	}
	auto seen = hasRoute.find( to );
	if( seen != end( hasRoute ) )
	{
		return seen->second;
	}

	auto found = m_references.find( to );
	if( found == end( m_references ) || IsOutdated( found->second ) )
	{
		hasRoute[to] = false;
		return false;
	}
	if( HasRoute(from, found->second.first.parent, hasRoute) )
	{
		hasRoute[to] = true;
		return true;
	}
	for( auto& ref : found->second.rest )
	{
		if( !IsOutdated( ref ) && HasRoute( from, ref.parent, hasRoute ) )
		{
			hasRoute[to] = true;
			return true;
		}
	}
	hasRoute[to] = false;
	return false;
}

bool AllReferences::IsOutdated( const Reference& ref ) const
{
	return m_cleaningReferences && ref.generation != m_generation;
}

bool AllReferences::IsOutdated( const References& refs ) const
{
	return m_cleaningReferences && refs.generation != m_generation;
}
