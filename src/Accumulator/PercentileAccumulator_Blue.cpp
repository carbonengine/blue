// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "PercentileAccumulator.h"

BLUE_DEFINE( PercentileAccumulator );

const Be::ClassInfo* PercentileAccumulator::ExposeToBlue()
{
	EXPOSURE_BEGIN(PercentileAccumulator, "For accumulating percentile stats, for example for FPS");
	MAP_METHOD_AND_WRAP
		(
		"Add",
		Add,
		"Report the metric value to the accumulator\n"
		":param value: new value"
		)
	MAP_METHOD_AND_WRAP
	(
		"Clear",
		Clear,
		"Reset the accumulator"
	)
	MAP_METHOD_AND_WRAP
	(
		"GetPercentiles",
		GetPercentiles,
		"Get the percentile values for the accumulated data, with the specified starting point and step size.\n"
		"Will return as many steps as necessary before reaching the max\n"
		":param start: starting value\n"
		":param step: step value"
	)
	MAP_METHOD_AND_WRAP
	(
		"GetValuesForPercentiles",
		GetValuesForPercentiles,
		"Get the percentile values for the accumulated data, with the specified step size and cutoff point.\n"
		"Will return as many steps as necessary counting down from 100% before reaching the cutoff point\n"
		"Values that are closer to the cut off point will be at the left of the returned list, while values to the right are closer to 100%\n"
		"Step size and cutoff point should be between 0.0 and 1.0 in size.\n"
		":param cutoff: cutoff point value\n"
		":param step: step size"
	)
	EXPOSURE_END();
}