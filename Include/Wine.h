////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Kristján Gerhardsson
// Created:		October 2019
// Copyright:	CCP 2019
//

#pragma once

#ifndef WINE_H
#define WINE_H

#include <string>

namespace Wine
{
	BLUEIMPORT bool IsWine();
	BLUEIMPORT const char* GetWineVersion();
	BLUEIMPORT const char* GetWineHostOs();
}

#endif