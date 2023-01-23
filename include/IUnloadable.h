////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		April 2014
// Copyright:	CCP 2014
//

#pragma once
#ifndef IUnloadable_h
#define IUnloadable_h

BLUE_INTERFACE( IUnloadable ) : public IRoot
{
	virtual void UnloadWhenUnreferenced() = 0;
	virtual void ReloadWhenReferenced() = 0;
};

#endif // IUnloadable_h