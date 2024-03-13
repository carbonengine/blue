/*
	*************************************************************************

	SchedulerCAPI.cpp

	Author:    Joseph Frangoudes
	Created:   March. 2024

	Description:
		
		SchedulerAPI() is the entrypoint for any blue code that wants to call any of
		scheduler's Python Capsule's functions. 

	Dependencies:

		SchedulerCAPI

	(c) CCP 2024
	*************************************************************************
*/

#include "SchedulerCAPI.h"

SchedulerCAPI* SchedulerAPI()
{
	static SchedulerCAPI* api;
	if (api == nullptr)
	{
		api = reinterpret_cast<SchedulerCAPI*>(PyCapsule_Import("scheduler._C_API", 0));
	}
	return api;
}
