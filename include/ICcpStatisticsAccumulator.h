// Copyright © 2013 CCP ehf.

#pragma once
#ifndef ICcpStatisticsAccumulator_h
#define ICcpStatisticsAccumulator_h

BLUE_INTERFACE_EXPORT( ICcpStatisticsAccumulator ) : public IRoot
{
	virtual void Add( double val ) = 0;
};

#endif // ICcpStatisticsAccumulator_h