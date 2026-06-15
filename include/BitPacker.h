// Copyright © 2011 CCP ehf.

#pragma once
#ifndef BitPacker_H
#define BitPacker_H

#ifdef WIN32
#pragma warning (disable : 4996) // remove Windows nagging about using _strincmp() etc
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include <string>

/*

 The two interfaces are BitPacker and BitPackerManaged, both provide
 similiar setup/constructor functions:

 BitPacker( const void* stream, const unsigned int maxLength );
 BitPackerManaged()

 The first (and reccomended) uses a provided buffer, while the second
 manages its own memory, both inherit form BitPackerCore and provide
 identical functionality for packing. A differnce comes again with the
 Finalize() call to round-up the buffer and pass it back if requested.

*/

const int c_defaultOwnedMemorySize = 512;

//------------------------------------------------------------------------------
class BitPackerCore
{
public:

	inline void Reset( bool release =false ); // if release is set true, the owned-memory will be reset to the default size

	inline bool DeQueueBits( char* target, int numberOfBits );
	inline bool DeQueueBytes( char* target, int numberOfBytes ) { return DeQueueBits(target, numberOfBytes * (8 * sizeof(char))); }
	inline bool DeQueueAlignedBlock( char** target, int numberOfBytes ); // passess back a pointer INTO THE BUFFER where the data resides

	inline bool QueueBits( char const* source, int numberOfBits );
	inline bool QueueBytes( char const* source, int numberOfBytes ) { return QueueBits(source, numberOfBytes * (8 * sizeof(char))); }
	inline bool QueueAlignedBlock( char const* source, int numberOfBytes ); // queues data guaranteed to be aligned to an 8-bit boundry (for recovery without a copy)
	inline unsigned int ReserveAlignedBlock( int numberOfBytes ); // return am offset to a reserved space in the bitstream numberOfBytes in size

	inline unsigned int Finalize( char **start =0 );
	inline void GetPosition( unsigned int& bytePos, unsigned int& bitPos ) { bitPos = m_bitsConsumed; bytePos = (unsigned int)(m_bitStream - m_firstBlock); }
	inline unsigned int GetBitPosition() { return m_bitsConsumed + ((sizeof(char)*8) * (unsigned int)(m_bitStream - m_firstBlock)); }

	inline bool Pack( const bool value );
	inline bool Pack( const uint32_t value );
	inline bool Pack( const int32_t value );
	inline bool Pack( const uint64_t value );
	inline bool Pack( const int64_t value );
	inline bool Pack( const char *string, const unsigned int len =0 );
	inline bool Pack( const std::string& string );
	inline bool Pack( const uint8_t c );
	inline bool Pack( const int8_t c );
	inline bool Pack( const wchar_t *wstring );
	inline bool Pack( const float value );
	inline bool Pack( const float value, unsigned int places );
	inline bool Pack( const double value );
	inline bool Pack( const double value, unsigned int places );

	inline bool Unpack( bool &value );
	inline bool Unpack( uint32_t &value );
	inline bool Unpack( int32_t &value );
	inline bool Unpack( uint64_t &value );
	inline bool Unpack( int64_t &value );
	inline bool Unpack( char *string, unsigned int *len =0, unsigned int maxLen =-1 );
	inline bool Unpack( std::string& string );
	inline bool Unpack( uint8_t &c );
	inline bool Unpack( int8_t &c );
	inline bool Unpack( wchar_t *wstring, unsigned int *len =0, unsigned int maxLen =-1 );
	inline bool Unpack( float &value );
	inline bool Unpack( float &value, unsigned int places );
	inline bool Unpack( double &value );
	inline bool Unpack( double &value, unsigned int places );

	unsigned int WordsRemaining() const { return m_lastBlock ? (unsigned int)(m_lastBlock - m_bitStream) + 1 : 0; }
	unsigned int WordsPacked() const { return (unsigned int)(m_bitStream - m_firstBlock); }

	bool Valid() { return !m_lastBlock || (m_lastBlock >= m_bitStream) || ((m_lastBlock == m_bitStream + 1) && m_bitsConsumed==0); }

protected:

	BitPackerCore( const void* stream, const unsigned int maxLength );
	~BitPackerCore() { if (m_ownedBufferSize) delete[] m_firstBlock; }


private:

	inline bool GrowOwnedMemory();

	unsigned int m_ownedBufferSize;
	unsigned int m_bitsConsumed;
	unsigned char *m_lastBlock;
	unsigned char *m_firstBlock;
	unsigned char *m_bitStream;
};

//------------------------------------------------------------------------------
class BitPacker : public BitPackerCore
{
public:
	// this version expects you to provide it with a sufficently large
	// chunk of memory and concerns itself only with preventing overflow
	BitPacker( const void* stream, const unsigned int maxLength ) : BitPackerCore( stream, maxLength ) { }
};

//------------------------------------------------------------------------------
class BitPackerManaged : public BitPackerCore
{
public:
	// this version manages its own memory
	BitPackerManaged() :BitPackerCore( 0, 0 ) { }
};

//------------------------------------------------------------------------------
inline BitPackerCore::BitPackerCore( const void* stream, const unsigned int maxLength )
{
	static_assert( sizeof(char) == 1 );

	m_bitsConsumed = 0;

	if ( stream )
	{
		m_ownedBufferSize = 0;
		m_firstBlock = (unsigned char*)stream;
		m_lastBlock = maxLength ? m_firstBlock + maxLength : 0;
	}
	else
	{
		m_ownedBufferSize = c_defaultOwnedMemorySize;
		m_firstBlock = new unsigned char[m_ownedBufferSize + 1];
		m_lastBlock = m_firstBlock + m_ownedBufferSize;
	}

	m_bitStream = m_firstBlock;
}

//------------------------------------------------------------------------------
void BitPackerCore::Reset( bool release /*=false*/ )
{
	m_bitsConsumed = 0;

	if ( release && m_ownedBufferSize )
	{
		delete[] m_firstBlock;
		m_ownedBufferSize = c_defaultOwnedMemorySize;
		m_firstBlock = new unsigned char[m_ownedBufferSize + 1];
		m_lastBlock = m_firstBlock + m_ownedBufferSize;
	}

	m_bitStream = m_firstBlock;
}

//------------------------------------------------------------------------------
bool BitPackerCore::DeQueueBits( char* target, int numberOfBits )
{
	if ( m_lastBlock && (m_bitStream >= m_lastBlock) )
	{
		return false;
	}

	int bitsRemaining = numberOfBits;
	unsigned char* buf = (unsigned char *)target;
	if ( !m_bitsConsumed )
	{
		// trivial case, on a boundary, no shifting required
		for(;;)
		{
			*buf = *m_bitStream;
			bitsRemaining -= sizeof(unsigned char) * 8;

			if ( bitsRemaining <= 0 )
			{
				m_bitsConsumed = (sizeof(unsigned char) * 8) + bitsRemaining;
				*buf &= (unsigned char)-1 >> ((sizeof(unsigned char) * 8) - m_bitsConsumed);

				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitStream++;
					m_bitsConsumed = 0;
				}

				break;
			}
			else
			{
				m_bitStream++;
				buf++;

				if ( m_lastBlock && m_bitStream >= m_lastBlock )
				{
					m_bitStream = m_lastBlock + 1;
					return false;
				}
			}
		}
	}
	else
	{
		for(;;)
		{
			*buf = *m_bitStream >> m_bitsConsumed;

			int compliment = (sizeof(unsigned char) * 8) - m_bitsConsumed;

			if ( bitsRemaining <= compliment )
			{
				*buf &= (unsigned char)-1 >> ((sizeof(unsigned char) * 8) - bitsRemaining);

				m_bitsConsumed += bitsRemaining;

				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitsConsumed = 0;
					m_bitStream++;

					// can't range check here, we don't know if this was
					// the last request
				}

				break;
			}

			bitsRemaining -= compliment;
			m_bitStream++;

			if ( (bitsRemaining > 0) && m_lastBlock && (m_bitStream >= m_lastBlock) )
			{
				m_bitStream = m_lastBlock + 1;
				return false;
			}

			*buf |= *m_bitStream << compliment;

			if ( bitsRemaining <= (int)m_bitsConsumed ) // was it enough?
			{
				m_bitsConsumed = bitsRemaining;
				*buf &= (unsigned char)-1 >> ((sizeof(unsigned char) * 8) - (compliment + bitsRemaining));

				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitsConsumed = 0;
					m_bitStream++;
				}

				break;
			}
			else
			{
				bitsRemaining -= m_bitsConsumed;
				buf++;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool BitPackerCore::DeQueueAlignedBlock( char** target, int numberOfBytes )
{
	if ( !target )
	{
		return false;
	}

	if ( m_bitsConsumed )
	{
		// first move the pointer up to the nearest boundry
		char dummy;
		if ( !DeQueueBits( &dummy, (sizeof(unsigned char) * 8) - m_bitsConsumed ) )
		{
			return false;
		}
	}

	*target = (char *)m_bitStream; // this is where the data lives

	// now advance the pointer past the aligned region
	m_bitStream += numberOfBytes;

	// NOT equal-to, since this might be the last piece of data and
	// legally point to the end of the buffer, the next de-queue will
	// fail if this is the case
	return !(m_lastBlock && (m_bitStream > m_lastBlock));
}

//------------------------------------------------------------------------------
bool BitPackerCore::GrowOwnedMemory()
{
	if ( !m_ownedBufferSize )
	{
		return false;
	}

	unsigned int offset = (unsigned int)(m_bitStream - m_firstBlock);
	unsigned char *newbuf = new unsigned char[(m_ownedBufferSize * 2) + 1];
	memcpy( newbuf, m_firstBlock, m_ownedBufferSize );
	delete[] m_firstBlock;
	m_firstBlock = newbuf;
	m_ownedBufferSize *= 2;
	m_lastBlock = m_firstBlock + m_ownedBufferSize;
	m_bitStream = m_firstBlock + offset;

	return true;
}

//------------------------------------------------------------------------------
bool BitPackerCore::QueueBits( char const* source, int numberOfBits )
{
	if ( m_lastBlock && m_bitStream >= m_lastBlock && !GrowOwnedMemory() )
	{
		m_bitStream = m_lastBlock + 1;
		return false;
	}

	int bitsRemaining = numberOfBits;
	unsigned char const* buf = (unsigned char *)source;

	if ( !m_bitsConsumed )
	{
		// trivial case, on a boundary, no shifting required
		for(;;)
		{
			*m_bitStream = *buf;
			bitsRemaining -= sizeof(unsigned char) * 8;

			if ( bitsRemaining <= 0 )
			{
				m_bitsConsumed = (sizeof(unsigned char) * 8) + bitsRemaining;
				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitStream++;
					m_bitsConsumed = 0;
				}

				break;
			}
			else
			{
				m_bitStream++;
				buf++;

				if ( m_lastBlock && m_bitStream >= m_lastBlock && !GrowOwnedMemory() )
				{
					m_bitStream = m_lastBlock + 1;
					return false;
				}
			}
		}
	}
	else
	{
		for(;;)
		{
			*m_bitStream = (*m_bitStream & ((unsigned char)-1 >> ((unsigned char)(sizeof(unsigned char) * 8) - m_bitsConsumed))) | (*buf << m_bitsConsumed);

			int compliment = (sizeof(unsigned char) * 8) - m_bitsConsumed;

			if ( bitsRemaining <= compliment ) // this was the last required write
			{
				m_bitsConsumed += bitsRemaining;
				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitsConsumed = 0;
					m_bitStream++;
				}

				break;
			}

			// up to next word in bitvector, and shift in the remaining bits from the buffer
			bitsRemaining -= compliment;
			m_bitStream++;

			if ( m_lastBlock && m_bitStream >= m_lastBlock && !GrowOwnedMemory() )
			{
				m_bitStream = m_lastBlock + 1;
				return false;
			}

			*m_bitStream = *buf >> compliment;

			if ( bitsRemaining <= (int)m_bitsConsumed ) // was it enough?
			{
				m_bitsConsumed = bitsRemaining;

				if ( m_bitsConsumed >= (sizeof(unsigned char) * 8) )
				{
					m_bitsConsumed = 0;
					m_bitStream++;
				}

				break;
			}
			else
			{
				bitsRemaining -= m_bitsConsumed;
				buf++;
			}
		}
	}

	return true;
}


//------------------------------------------------------------------------------
bool BitPackerCore::QueueAlignedBlock( char const* source, int numberOfBytes )
{
	if ( m_bitsConsumed )
	{
		// first move the pointer up to the nearest boundry
		char dummy = 0;
		if ( !QueueBits( &dummy, (sizeof(unsigned char) * 8) - m_bitsConsumed ) )
		{
			return false;
		}
	}

	// then just copy it in
	return QueueBits( source, numberOfBytes * (8 * sizeof(char)) );
}
/*
//------------------------------------------------------------------------------
bool BitPackerCore::QueueAlignedBlock( char const* source, int numberOfBytes )
{
	if ( m_bitsConsumed )
	{
		// move up to next even boundary
		m_bitsConsumed;
		m_bitStream++;
		if ( m_lastBlock && m_bitStream >= m_lastBlock && !GrowOwnedMemory() )
		{
			m_bitStream = m_lastBlock + 1;
			return false;
		}
	}

	// now copy it in
	return QueueBits( source, numberOfBytes * 8 );
}
*/
//------------------------------------------------------------------------------
unsigned int BitPackerCore::ReserveAlignedBlock( int numberOfBytes )
{
	if ( m_bitsConsumed )
	{
		m_bitsConsumed = 0;
		m_bitStream++;
	}

	unsigned int offset = (unsigned int)(m_bitStream - m_firstBlock);

	m_bitStream += numberOfBytes;
	if ( m_lastBlock && m_bitStream >= m_lastBlock && !GrowOwnedMemory() )
	{
		m_bitStream = m_lastBlock + 1;
		return 0;
	}

	return offset;
}

//------------------------------------------------------------------------------
unsigned int BitPackerCore::Finalize( char **start )
{
	if ( m_bitsConsumed > 0 )
	{
		// unassigned bits shall be set to '0' after finalize
		*m_bitStream &= (unsigned char)-1 >> ((sizeof(unsigned char)*8) - m_bitsConsumed);
		m_bitStream++;
		m_bitsConsumed = 0;
	}

	if ( start )
	{
		*start = (char *)m_firstBlock;
	}

	unsigned int ret = (unsigned int)(m_bitStream - m_firstBlock);
	return ret;
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const bool value )
{
	char bit = value ? 1 : 0;
	return QueueBits( (char*)&bit, 1 );
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const uint32_t value )
{
	uint32_t j = 1;

	if ( value == 0 ) // zero is special case, queue the 3 bits and be done
	{
		QueueBits( (char*)&value, 3 );
		return Valid();
	}
	else
	{
		for( ; j<7 ; j++ )
		{
			if ( value < (uint32_t)(0x1 << (j*4)) )
			{
				QueueBits( (char*)&j, 3 );
				QueueBits( (char*)&value, (j*4) );
				return Valid();
			}
		}
	}

	QueueBits( (char*)&j, 3 );
	QueueBits( (char*)&value, 32 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const int32_t value )
{
	if ( value == 0 )
	{
		QueueBits( (char*)&value, 1 );
		Pack( (uint32_t)0 );
	}
	else if ( value > 0 )
	{
		uint32_t val = 0;
		QueueBits( (char*)&val, 1 );
		Pack( (uint32_t)value );
	}
	else
	{
		uint32_t val = 1;
		QueueBits( (char*)&val, 1 );
		val = -value;
		Pack( val );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const uint64_t value )
{
	uint64_t j = 1;

	if ( value == 0 ) // zero is special case, queue the 3 bits and be done
	{
		QueueBits( (char*)&value, 3 );
		return Valid();
	}
	else
	{
		for( ; j<7 ; j++ )
		{
			if ( value < (unsigned long long)(0x1LL << (j*5)) )
			{
				QueueBits( (char*)&j, 3 );
				QueueBits( (char*)&value, (int)(j*5) );
				return Valid();
			}
		}
	}

	j = 0xFFFFFFFF;
	QueueBits( (char*)&j, 3 );
	QueueBits( (char*)&value, 64 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const int64_t value )
{
	if ( value == 0 )
	{
		QueueBits( (char*)&value, 1 );
		Pack( 0ULL );
	}
	else if ( value > 0 )
	{
		uint64_t val = 0;
		QueueBits( (char*)&val, 1 );
		Pack( (uint64_t)value );
	}
	else
	{
		uint64_t val = 1;
		QueueBits( (char*)&val, 1 );
		val = -value;
		Pack( val );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const char *string, const unsigned int len /*=0*/ )
{
	if ( !string )
	{
		return false;
	}

	unsigned int size = len ? len : (unsigned int)strlen(string);
	Pack( size );
	for( unsigned int i=0; i<size; i++)
	{
		QueueBits( string++, 8 );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const std::string& string )
{
	Pack( (unsigned int)string.length() );
	for( unsigned int i=0; i<string.length(); i++)
	{
		QueueBits( string.c_str()+ i, 8 );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const int8_t c )
{
	return QueueBits( (const char *)&c, 8 );
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const uint8_t c )
{
	return QueueBits( (const char *)&c, 8 );
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const wchar_t *string )
{
	if ( !string )
	{
		return false;
	}

	char temp[64001];

	int len = (unsigned int)wcstombs( temp, string, 64000 );
	Pack( len );
	if ( len != 0 )
	{
		Pack( temp );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const float value )
{
/*
	// 0x0 - zero, no bits follow
	// 0x1 - positive, pack follows
	// 0x2 - negative, pack follows
	// 0x3 - 32 literal bits follow

	// mark zero with a single bit
	if ( value == 0 )
	{
		QueueBits( (char*)&value, 2 );
		return Valid();
	}

	const unsigned int *f = (unsigned int*)&value;

	unsigned int sign = *f & 0x80000000 ? 1 : 0;
	int exponent = (int)((*f & 0x7F800000) >> 23) - 127;

	if ( (exponent < 0) || exponent > 15 )
	{
		// assume bad news, just pack 31 more bits and be done
		sign = 0x3;
		QueueBits( (char *)&sign, 2 ); // mark full precision
		QueueBits( (char *)f, 32 );
		return Valid();
	}

	if ( sign ) // negative
	{
		sign = 0x2;
		QueueBits( (char *)&sign, 2 );
	}
	else
	{
		sign = 0x1;
		QueueBits( (char *)&sign, 2 );
	}

	Pack( (char)exponent ); // pack as char

	unsigned int mantissa = (*f & 0x007FFFFF);
	if ( !mantissa )
	{
		mantissa = 0x7;
		QueueBits( (char *)&mantissa, 3 );
		return Valid();
	}

	int i=1;
	for( ; i<7 ; i++ )
	{
		if ( mantissa != ((mantissa >> i*4) << i*4) )
		{
			i--;
			QueueBits( (char *)&i, 3 );
			int temp = mantissa >> i*4;
			QueueBits( (char *)&temp, 23 - (i * 4) );

			return Valid();
		}
	}
*/
	QueueBits( (char *)&value, 32 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const float value, unsigned int places )
{
	unsigned int val = 0;
	if ( value == 0 )
	{
		QueueBits( (char*)&val, 1 );
		return Valid();
	}

	val = 1;
	QueueBits( (char*)&val, 1 );

	val = (unsigned int)(value * (float)(1<<places) );

	return Pack( val );
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const double value )
{
	QueueBits( (char*)&value, 64 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Pack( const double value, unsigned int places )
{
	unsigned long long val = 0;
	if ( value == 0 )
	{
		QueueBits( (char*)&val, 1 );
		return Valid();
	}

	val = 1;
	QueueBits( (char*)&val, 1 );

	val = (unsigned long long)(value * (double)(1UL<<places));
	return Pack( val );
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( bool &value )
{
	char bit = 0;
	DeQueueBits( (char*)&bit, 1 );
	value = bit == 1 ? true : false;

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( unsigned int &value )
{
	value = 0;
	DeQueueBits( (char*)&value, 3 );

	if ( value != 0 )
	{
		DeQueueBits( (char*)&value, value == 7 ? 32 : value * 4 );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( int &value )
{
	value = 0;
	DeQueueBits( (char *)&value, 1 );
	if ( value )
	{
		Unpack( (unsigned int&)value );
		value = -value;
	}
	else
	{
		Unpack( (unsigned int&)value );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( unsigned long long &value )
{
	value = 0;
	DeQueueBits( (char*)&value, 3 );

	if ( value != 0 )
	{
		DeQueueBits( (char*)&value, (int)(value == 7 ? 64 : value * 5) );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( long long &value )
{
	value = 0;
	DeQueueBits( (char *)&value, 1 );
	if ( value )
	{
		Unpack( (unsigned long long&)value );
		value = -value;
	}
	else
	{
		Unpack( (unsigned long long&)value );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( char *string, unsigned int *len, unsigned int maxLen )
{
	if ( !string )
	{
		return false;
	}
	unsigned int tlen;
	Unpack( tlen );

	if ( tlen > maxLen )
	{
		return false;
	}

	if ( len )
	{
		*len = tlen;
	}

	for( unsigned int i=0; i<tlen; i++ )
	{
		DeQueueBits( string++, 8 );
	}
	*string = 0; // terminate

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( std::string& string )
{
	unsigned int len;
	Unpack( len );

	string.clear();
	string.reserve( len );
	char c;

	for( unsigned int i=0; i<len; i++ )
	{
		DeQueueBits( &c, 8 );
		string.push_back( c );
	}

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( wchar_t *string, unsigned int *len, unsigned int maxLen )
{
	unsigned int tlen;

	Unpack( *(int *)&tlen );
	if ( tlen == 0 )
	{
		if ( len )
		{
			*len = 0;
		}
		string[0] = 0;
		return true;
	}

	if ( tlen > maxLen )
	{
		return false;
	}

	char temp[64001];
	Unpack( temp );

	tlen = (unsigned int)mbstowcs( string, temp, 32000 );
	if ( len )
	{
		*len = tlen;
	}

	return (tlen != (unsigned int)-1) && Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( uint8_t &c )
{
	DeQueueBits( (char*)&c, 8 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( int8_t &c )
{
	DeQueueBits( (char*)&c, 8 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( float &value )
{
/*
	value = 0;
	unsigned int *f = (unsigned int*)&value;
	unsigned int temp = 0;
	DeQueueBits( (char*)&temp, 2 );
	if ( temp == 0x0 )
	{
		// 0x0 - zero
		return Valid();
	}
	else if ( temp == 0x3 )
	{
		// 0x3 - literal
		DeQueueBits( (char *)&value, 32 );
	}
	else
	{
		// 0x1 or 0x2 (2 being negative)
		if ( temp == 0x2 )
		{
			*f |= 0x80000000;
		}

		temp = 0;
		Unpack( (char &)temp );
		temp += 127;
		*f |= temp << 23;

		int bits = 0;
		DeQueueBits( (char *)&bits, 3 );
		if ( bits < 7 )
		{
			DeQueueBits( (char *)&temp, 23 - (bits * 4) );
			*f |= temp << bits * 4;
		}
	}
*/
	DeQueueBits( (char *)&value, 32 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( float &value, unsigned int places )
{
	unsigned int val = 0;
	DeQueueBits( (char*)&val, 1 );

	if ( val == 0 )
	{
		value = 0.f;
		return Valid();
	}

	Unpack( val );
	value = (float)val / (float)(1<<places);

	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( double &value )
{
	DeQueueBits( (char*)&value, 64 );
	return Valid();
}

//------------------------------------------------------------------------------
bool BitPackerCore::Unpack( double &value, unsigned int places )
{
	unsigned long long val = 0;
	DeQueueBits( (char*)&val, 1 );

	if ( val == 0 )
	{
		value = 0.f;
		return Valid();
	}

	Unpack( val );
	value = (double)val / (double)(1UL<<places);

	return Valid();
}

#endif
