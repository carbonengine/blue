// Copyright © 2014 CCP ehf.

#pragma once
#ifndef PercentileAccumulator_h
#define PercentileAccumulator_h

#include "ICcpStatisticsAccumulator.h"

BLUE_DECLARE( PercentileAccumulator );

class PercentileAccumulator : public ICcpStatisticsAccumulator
{
public:
	EXPOSE_TO_BLUE();
	PercentileAccumulator();
	void Add( double val ) override;
	void Clear();
	Be::Result<std::string> GetPercentiles(double start, double step, std::list<double>& percentiles);
	Be::Result<std::string> GetValuesForPercentiles(double cutoff_point, double step_size, std::list<double>& percentiles);
private:
	std::vector<double> m_data;
};

TYPEDEF_BLUECLASS( PercentileAccumulator )

#endif
