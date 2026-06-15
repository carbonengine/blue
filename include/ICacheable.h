// Copyright © 2014 CCP ehf.

#ifndef ICacheable_h__
#define ICacheable_h__

#include "Blue.h"

BLUE_INTERFACE( ICacheable ) : public IRoot
{
	// Is the memory usage known? Sometimes it isn't, as in the case of
	// asynchronously loaded resources - memory usage isn't known until
	// loading has finished.
	virtual bool IsMemoryUsageKnown() = 0;

	// Return the memory usage of this object. Once IsMemoryUsageKnown()
	// returns true, the number returned here should not change.
	virtual size_t GetMemoryUsage() = 0;
};

// Use this macro to add Python exposure to the methods of ICacheable
#define MAP_ICACHEABLE_METHODS() \
	MAP_METHOD_AND_WRAP( "IsMemoryUsageKnown", IsMemoryUsageKnown, \
		"Is the memory usage known?" ) \
	MAP_METHOD_AND_WRAP( "GetMemoryUsage", GetMemoryUsage, \
		"Return the memory usage of the object." )

#endif // ICacheable_h__