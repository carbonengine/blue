// Copyright © 2014 CCP ehf.

#pragma once

#ifndef IBLUEOBJECTPROXY_H
#define IBLUEOBJECTPROXY_H

#include "IBlueObjectBuilder.h"

BLUE_INTERFACE( IBlueObjectProxy ) : public IRoot
{
	virtual void SetBuilder( IBlueObjectBuilder* builder, unsigned int objectMarker ) = 0;
	virtual IRoot* GetObject( ) = 0;
	virtual bool IsResident() const = 0;
	virtual void Freeze() = 0;
	virtual void ClearObject() = 0;
	virtual bool Update( Be::Time time, Be::Time timeout ) = 0;
	virtual void SetObject( IRoot* obj ) = 0;
	virtual void SetObjectFromBuilder( IRoot* obj ) = 0;

	// Set a flag that helps the user of the proxy realize that even though IsResident()
	// may be true, the object is just a temporary placeholder.
	virtual void SetTemporary( bool isTemporary ) = 0;
	virtual bool IsTemporary() const = 0;

	// Notification telling this proxy that its object is in use
	virtual void OnSelected() = 0;
};

#endif
