/*
	*************************************************************************

	SchedulerCAPI.h

	Author:    Joseph Frangoudes
	Created:   March. 2024
	Project:   Blue

	Description:

		SchedulerAPI() is the entrypoint for any blue code that wants to call any of
		scheduler's Python Capsule's functions.


	Dependencies:

		Scheduler, Python

	(c) CCP 2024

	*************************************************************************
*/

#pragma once

#ifndef _SCHEDULERCAPI_H_
#define _SCHEDULERCAPI_H_

#include <Python.h>
#include <scheduler.h>

/**
 * PyCapsule_Import will set an exception if there's an error.
 *
 * @return SchedulerCAPI * or nullptr on error
 */
SchedulerCAPI* SchedulerAPI();


#endif
