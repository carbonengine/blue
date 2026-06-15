// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "PercentileAccumulator.h"

PercentileAccumulator::PercentileAccumulator()
{
	m_data.reserve(32768); // Approximately we could run at 60 fps for 9 minutes before doubling the size of the buffer.
}

void PercentileAccumulator::Add(double val)
{
	m_data.push_back(val);
}

void PercentileAccumulator::Clear()
{
	m_data.clear();
}

Be::Result<std::string> PercentileAccumulator::GetPercentiles(double start, double step, std::list<double>& percentiles )
{
	int no_values = (int) m_data.size();

	if( no_values == 0 )
	{
		return Be::Result<std::string>();
	}

	std::sort(m_data.begin(), m_data.end());
	double max = *m_data.rbegin();
	if( max < start )
	{
		return Be::Result<std::string>();
	}
	int no_steps = (int) ceil((max - start) / step ) + 1;  // Add one because there is a step at both beginning and at end.
	
	
	std::vector<int> step_counts(no_steps, 0); // How many of the accumulated values fall above the steps lower bound
	double lower_bound = start;
	
	for(int i = 0; i < no_steps; ++i)
	{
		for(auto it = m_data.rbegin(); it != m_data.rend(); ++it)
		{
			if( *it > lower_bound )
			{
				step_counts[i] += 1;
			}
			else
			{
				break;
			}
		}
		double percentile_for_step = ((double)step_counts[i]) / ((double)no_values);
		percentiles.push_front( percentile_for_step );
		lower_bound += step;
	}

	return Be::Result<std::string>();
}

Be::Result<std::string> PercentileAccumulator::GetValuesForPercentiles(double cutoff_point, double step_size, std::list<double>& percentiles )
{
	int no_values = (int) m_data.size();

	if( no_values == 0 )
	{
		return Be::Result<std::string>();
	}

	if( cutoff_point > 1.0 || cutoff_point < 0.0 )
	{
		return Be::Result<std::string>("Error, cutoff point should be in the range 0 to 1.");
	}

	if( step_size > 1.0 || step_size < 0.0 )
	{
		return Be::Result<std::string>("Error, step size should be in the range 0 to 1.");
	}

	std::sort(m_data.begin(), m_data.end());
	
	int no_steps = (int) ceil((1.0 - cutoff_point) / step_size ) + 1;  // Add one because there is a step at both beginning and at end.
	
	for(int i = 0; i < no_steps; ++i)
	{
		int index = (int)(no_values - (i * step_size * no_values) - 0.5);
		percentiles.push_front(m_data[index]);
	}

	return Be::Result<std::string>();
}