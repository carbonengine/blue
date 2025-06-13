#include "StdAfx.h"

#if CCP_STACKLESS

//
// Custom memory allocators for Python
//
// These are primarily used to track statistics about memory allocated by Python.
//

CCP_STATS_DECLARE( pyMemory, "Blue/Memory/Python", false, CST_MEMORY, "The amount of memory allocated for Python" );

struct MeasuredAllocator
{
	MeasuredAllocator() :
		measurement{ g_ccpStatistics_pyMemory } {};
	PyMemAllocatorEx allocator;
	CcpStaticStatisticsEntry& measurement;
};

namespace Ccp
{

void* MeasuredMalloc( void* ctx, size_t size )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	auto ret = _this->allocator.malloc( _this->allocator.ctx, size );
#if _WIN32
	_this->measurement.Add( int64_t( size ) );
#elif __APPLE__
	_this->measurement.Add( int64_t( CCPMSize( ret ) ) );
#else
#error "Unsupported platform"
#endif
	return ret;
}

void* MeasuredCalloc( void* ctx, size_t nelem, size_t size )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	auto ret = _this->allocator.calloc( _this->allocator.ctx, nelem, size );
#if _WIN32
	_this->measurement.Add( int64_t( nelem * size ) );
#elif __APPLE__
	_this->measurement.Add( int64_t( CCPMSize( ret ) ) );
#else
#error "Unsupported platform"
#endif
	return ret;
}

void* MeasuredRealloc( void* ctx, void* ptr, size_t newSize )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	uint64_t prev = CCPMSize( ptr );
	auto ret = _this->allocator.realloc( _this->allocator.ctx, ptr, newSize );
#if _WIN32
	_this->measurement.Add( int64_t( newSize ) - prev );
#elif __APPLE__
	_this->measurement.Add( int64_t( CCPMSize( ret ) - prev ) );
#else
#error "Unsupported platform"
#endif
	return ret;
}

void MeasuredFree( void* ctx, void* ptr )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	_this->measurement.Add( -int64_t( CCPMSize( ptr ) ) );
	_this->allocator.free( _this->allocator.ctx, ptr );
}
}

extern "C" BLUEIMPORT void BlueInstallPythonMemoryHooks()
{
	static std::array<MeasuredAllocator, 3> measurers;
	static std::array<PyMemAllocatorEx, 3> overrides{
		{ { &measurers[PYMEM_DOMAIN_RAW], Ccp::MeasuredMalloc, Ccp::MeasuredCalloc, Ccp::MeasuredRealloc, Ccp::MeasuredFree },
		  { &measurers[PYMEM_DOMAIN_MEM], Ccp::MeasuredMalloc, Ccp::MeasuredCalloc, Ccp::MeasuredRealloc, Ccp::MeasuredFree },
		  { &measurers[PYMEM_DOMAIN_OBJ], Ccp::MeasuredMalloc, Ccp::MeasuredCalloc, Ccp::MeasuredRealloc, Ccp::MeasuredFree } }
	};

	PyMemAllocatorEx temporaryHelper;

	PyMem_GetAllocator( PYMEM_DOMAIN_RAW, &temporaryHelper );
	if( temporaryHelper.ctx != &overrides[PYMEM_DOMAIN_RAW] )
	{
		measurers[PYMEM_DOMAIN_RAW].allocator = temporaryHelper;
		PyMem_SetAllocator( PYMEM_DOMAIN_RAW, &overrides[PYMEM_DOMAIN_RAW] );
	}

	PyMem_GetAllocator( PYMEM_DOMAIN_MEM, &temporaryHelper );
	if( temporaryHelper.ctx != &overrides[PYMEM_DOMAIN_MEM] )
	{
		measurers[PYMEM_DOMAIN_MEM].allocator = temporaryHelper;
		PyMem_SetAllocator( PYMEM_DOMAIN_MEM, &overrides[PYMEM_DOMAIN_MEM] );
	}

	PyMem_GetAllocator( PYMEM_DOMAIN_OBJ, &temporaryHelper );
	if( temporaryHelper.ctx != &overrides[PYMEM_DOMAIN_OBJ] )
	{
		measurers[PYMEM_DOMAIN_OBJ].allocator = temporaryHelper;
		PyMem_SetAllocator( PYMEM_DOMAIN_OBJ, &overrides[PYMEM_DOMAIN_OBJ] );
	}
}

#endif
