// Copyright © 2011 CCP ehf.

#pragma once
#ifndef LINKHASH_HPP
#define LINKHASH_HPP

#include <new>

namespace Ccp
{

//-----------------------------------------------------------------------------
// a very fast/simple hashing algorithm based ont he FNV hash. Note
// that the literal values ARE magical, do not change them!
inline unsigned int Hash( const void *dat, const unsigned int len, unsigned int pre =0x811C9DC5 )
{
	unsigned int length = len;
	unsigned char *data = (unsigned char *)dat;
	while ( length-- )
	{
		pre ^= (unsigned int)(*data++);
		pre *= 0x1000193;
	}

	return pre;
}

//-----------------------------------------------------------------------------
// simplified version that assumes a null-terminated string
inline unsigned int HashString( const char *string, unsigned int pre =0x811C9DC5 )
{
	while ( *string )
	{
		pre ^= (unsigned int)(*string++);
		pre *= 0x1000193;
	}

	return pre;
}

//------------------------------------------------------------------------------
inline unsigned long long Hash64( const void *dat, const unsigned int len, unsigned long long pre =0xCBF29CE484222325ULL )
{
	unsigned int length = len;
	unsigned char *data = (unsigned char *)dat;
	while ( length-- )
	{
		pre ^= (unsigned long long)(*data++);
		pre *= 0x100000001B3ULL;
	}

	return pre;
}

//------------------------------------------------------------------------------
inline unsigned long long Hash64String( const char *string, unsigned long long pre =0xCBF29CE484222325ULL )
{
	while ( *string )
	{
		pre ^= (unsigned long long)(*string++);
		pre *= 0x100000001B3ULL;
	}

	return pre;
}

//-----------------------------------------------------------------------------
// LinkHash is a highly optimized hash list which has some very
// important properties:
//
// - node-memory is pooled, and released only when the list is destroyed
// - hash entries maintain a linked-list of all entries, so iterating
//   is very fast
// - in-place iteration
// - no effort is wasted making this list thread-safe, that's up to you
//   if necessary
template <class H> class LinkHash
{
public:

	inline H* add( const unsigned long long key );
	void addItem( H item, const unsigned long long key );
	inline bool remove( const unsigned long long key );
	inline void clear();
	unsigned int count() const { return m_count; }
	inline void resize( unsigned int newBucketCount );
	H* getFirst() { m_current = m_head; return m_current ? &(m_current->item) : 0; }
	H* getNext() { m_current = m_current ? m_current->nextIter : m_head; return getCurrent(); }
	H* getCurrent() { return m_current ? &(m_current->item) : 0; }
	unsigned long long getCurrentKey() { return m_current ? m_current->key : 0; }

	H* get( const unsigned long long key ) const
	{
		for( const SNode *N = m_list[key % m_mod]; N ; N=N->next )
		{
			if ( N->key == key )
			{
				return (H *)&(N->item);
			}
		}
		return 0;
	}

	H getItem( const unsigned long long key ) const
	{
		for( const SNode *N = m_list[key % m_mod]; N ; N=N->next )
		{
			if ( N->key == key )
			{
				return N->item;
			}
		}
		return 0;
	}

	LinkHash( bool pool =true ) { m_pool=pool; m_head=0; m_current=0; m_list=0; m_mod=0; m_count=0; m_freeList=0; resize(4); }
	~LinkHash() { clear(); delete[] m_list; while(m_freeList) { SNode *N=m_freeList->next; delete m_freeList; m_freeList=N; } }

private:
	struct SNode
	{
		H item;
		unsigned long long key;
		SNode *next; // next in the hash link chain
		SNode *nextIter; // list is doubly-linked
		SNode *prevIter;
	};

public:
	class CIterator
	{
	public:
		CIterator( const LinkHash<H> &list ) { m_list = &list; m_current = 0; }
		inline void removeCurrent();
		H* getFirst() { m_current = m_list->m_head; return m_current ? &(m_current->item) : 0; }
		H* getNext() { m_current = m_current ? m_current->nextIter : m_list->m_head; return m_current ? &(m_current->item) : 0; }
		unsigned long long getCurrentKey() { return m_current ? m_current->key : 0; }
	private:
		const LinkHash<H> *m_list;
		SNode *m_current;
	};

private:
	SNode *m_head;
	SNode *m_current;
	SNode *m_freeList;
	bool m_pool;
	SNode **m_list;
	unsigned int m_mod;
	unsigned int m_count;
	unsigned int m_loadThreshold;
};

//-----------------------------------------------------------------------------
template<class H> void LinkHash<H>::CIterator::removeCurrent()
{
	if ( !m_current )
	{
		return;
	}

	SNode* newEntry = m_current->prevIter; // make sure the list stays sane after the removal
	m_list->remove( m_current->key );
	m_current = newEntry;
}

//-----------------------------------------------------------------------------
template<class H> H* LinkHash<H>::add( const unsigned long long key )
{
	const int p = (int)(key % m_mod);

	SNode *N;
	if ( !m_freeList || !m_pool )
	{
		N = new(std::nothrow) SNode;
	}
	else
	{
		N = m_freeList;
		m_freeList = N->next;
	}

	N->key = key;
	N->next = m_list[p];
	m_list[p] = N;

	if ( m_head )
	{
		m_head->prevIter = N;
	}
	N->nextIter = m_head;
	N->prevIter = 0;
	m_head = N;

	m_count++;

	if ( m_count > m_loadThreshold )
	{
		resize( m_mod * 2 );
	}

	return &N->item;
}

//-----------------------------------------------------------------------------
template<class H> void LinkHash<H>::addItem( H item, const unsigned long long key )
{
	const int p = (int)(key % m_mod);

	SNode *N;
	if ( !m_freeList || !m_pool )
	{
		N = new(std::nothrow) SNode;
	}
	else
	{
		N = m_freeList;
		m_freeList = N->next;
	}

	N->item = item;
	N->key = key;
	N->next = m_list[p];
	m_list[p] = N;

	if ( m_head )
	{
		m_head->prevIter = N;
	}
	N->nextIter = m_head;
	N->prevIter = 0;
	m_head = N;

	m_count++;

	if ( m_count > m_loadThreshold )
	{
		resize( m_mod * 2 );
	}
}

//-----------------------------------------------------------------------------
template<class H> bool LinkHash<H>::remove( unsigned long long key )
{
	// find the position
	const int p = (int)(key % m_mod);

	// iterate through, find/unlink the item
	SNode *N = 0;
	for( SNode *E = m_list[p]; E ; E=E->next )
	{
		if ( E->key == key )
		{
			if ( E == m_current )
			{
				m_current = E->prevIter;
			}

			if ( N )
			{
				N->next = E->next;
			}
			else
			{
				m_list[p] = E->next;
			}

			if ( E == m_head )
			{
				m_head = E->nextIter;

				if ( m_head )
				{
					m_head->prevIter = 0;
				}
			}
			else
			{
				if ( E->prevIter )
				{
					E->prevIter->nextIter = E->nextIter;
				}

				if ( E->nextIter )
				{
					E->nextIter->prevIter = E->prevIter;
				}
			}

			if ( m_pool )
			{
				E->next = m_freeList;
				m_freeList = E;
			}
			else
			{
				delete E;
			}

			m_count--;
			return true;
		}

		N = E;
	}

	return false;
}

//-----------------------------------------------------------------------------
template<class H> void LinkHash<H>::resize( unsigned int newBucketCount )
{
	m_mod = newBucketCount < 4 ? 4 : newBucketCount;

	SNode **oldList = m_list;
	m_list = new(std::nothrow) SNode*[ m_mod ];
	memset( m_list, 0, m_mod * sizeof(SNode*) );

	for( SNode *N = m_head ; N ; N = N->nextIter )
	{
		int bucket = (int)(N->key % m_mod);
		N->next = m_list[bucket];
		m_list[bucket] = N;
	}

	m_loadThreshold = m_mod - (m_mod / 4);
	delete[] oldList;
}

//-----------------------------------------------------------------------------
template<class H> void LinkHash<H>::clear()
{
	for( unsigned int l=0; l<m_mod ; l++ )
	{
		while( m_list[l] )
		{
			SNode *N = m_list[l]->next;
			delete m_list[l];
			m_list[l] = N;
		}
	}

	m_head = 0;
	m_current = 0;
	m_count = 0;
}

} // namespace

#endif
