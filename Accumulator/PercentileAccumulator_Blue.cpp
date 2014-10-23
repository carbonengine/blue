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
		"Report the metric value to the accumulator"
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
		"Will return as many steps as necessary before reaching the max"
	)
	EXPOSURE_END();
}