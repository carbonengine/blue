////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#pragma once
#ifndef ICcpStatisticsAccumulator_h
#define ICcpStatisticsAccumulator_h

BLUE_INTERFACE_EXPORT( ICcpStatisticsAccumulator ) : public IRoot
{
	virtual void Add( double val ) = 0;
};

#endif // ICcpStatisticsAccumulator_h