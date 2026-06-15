// Copyright © 2008 CCP ehf.

/* 
	*************************************************************************

	IBluePlacementObserver.h

	Project:   Blue

	Description:   

		Interface to enable other modules to push positions and
		orientation to other modules. Originally for audio2.


	Dependencies:

		Blue

	*************************************************************************
*/

#pragma once
#ifndef _IBLUEPLACEMENTOBSERVER_H_
#define _IBLUEPLACEMENTOBSERVER_H_

#include "Blue.h"

struct Vector3;

BLUE_INTERFACE( IBluePlacementObserver ) : public IRoot
{
	BLUEIMPORT virtual void UpdatePlacement( const Vector3& front_, const Vector3& top_, const Vector3& pos_ ) = 0;
};

struct PositionDescription
{
	float front_x;
	float front_y;
	float front_z;
	float pos_x;
	float pos_y;
	float pos_z;
};

typedef std::vector< PositionDescription > PositionDescriptionVector;

BLUE_INTERFACE( IBlueMultiPlacementObserver ) : public IRoot
{
	BLUEIMPORT virtual void UpdatePlacements( const PositionDescriptionVector& positions ) = 0;
};

#endif
