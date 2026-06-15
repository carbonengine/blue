// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BLUEOBJECTPROXY_H
#define BLUEOBJECTPROXY_H

#include "IBlueObjectProxy.h"

BLUE_DECLARE( BlueObjectProxy );

class BlueObjectProxy : public IBlueObjectProxy
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectProxy();
	~BlueObjectProxy();

	//////////////////////////////////////////////////////////////////////////
	// IBlueObjectProxy
	void SetBuilder( IBlueObjectBuilder* builder, unsigned int objectMarker ) override;
	IRoot* GetObject( ) override;
	bool IsResident() const override;
	void Freeze() override;
	void ClearObject() override;
	bool Update( Be::Time time, Be::Time timeout ) override;
	void SetObject( IRoot* obj ) override;

	// Methods to facilitate async updates
	// Replace the object in the proxy without unlinking the builder
	void SetObjectFromBuilder( IRoot* obj ) override;
	// Set a flag that helps the user of the proxy realize that even though IsResident()
	// may be true, the object is just a temporary placeholder.
	void SetTemporary( bool isTemporary ) override;
	bool IsTemporary() const override;
	void OnSelected() override;

protected:
	IBlueObjectBuilderPtr m_builder;
	unsigned int m_objectMarker;
	IRootPtr m_object;
	Be::Time m_lastTimeUsed;

	// called anytime the old value of object is about to change
	void OnObjectInvalidated();
	bool m_temporary;

	bool m_isUnloaded;
};

TYPEDEF_BLUECLASS( BlueObjectProxy );

#endif
