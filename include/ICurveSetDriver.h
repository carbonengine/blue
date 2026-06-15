// Copyright © 2023 CCP ehf.

#pragma once

// Drive a CurveSet by anything else other than time. 
BLUE_INTERFACE( ICurveSetDriver ) : public IRoot
{
	virtual double GetCurveSetTime( double time ) = 0;
};
