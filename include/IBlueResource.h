// Copyright © 2014 CCP ehf.

#pragma once

#ifndef IBLUERESOURCE_H
#define IBLUERESOURCE_H

#include "Blue.h"

typedef void (*BlueResourceNotifyFunc)( struct IBlueResource* p );

BLUE_INTERFACE( IBlueResource ) : public IRoot
{
	virtual void Initialize( const wchar_t* name, const wchar_t* ext ) = 0;

	virtual const wchar_t* GetPath() const = 0;
	virtual const wchar_t* GetExt() const = 0;

	// Returns true as soon as load phase is done - note that prepare may still be pending
	virtual bool IsLoading() const = 0;

	// Returns true as soon as prepare has finished. Errors in loading or prepare phase
	// are indicated by IsGood returning false.
	virtual bool IsPrepared() const = 0;

	// Returns true if the resource has loaded and prepared successfully. Note that a resource
	// can be prepared but not good if any errors occurred.
	virtual bool IsGood() const = 0;
};

#endif
