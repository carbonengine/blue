#pragma once
#ifndef BLUEOBJECTPROXY_H
#define BLUEOBJECTPROXY_H

#include "include/IBlueObjectProxy.h"

BLUE_DECLARE( BlueObjectProxy );

class BlueObjectProxy : public IBlueObjectProxy
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectProxy();
	~BlueObjectProxy();

	//////////////////////////////////////////////////////////////////////////
	// IBlueObjectProxy
	void SetBuilder( IBlueObjectBuilder* builder, unsigned int objectMarker );
	IRoot* GetObject( );
	bool IsResident() const;
	void Freeze();
	void ClearObject();
	bool Update( Be::Time time, Be::Time timeout );
	void SetObject( IRoot* obj );

	// Methods to facilitate async updates
	// Replace the object in the proxy without unlinking the builder
	void SetObjectFromBuilder( IRoot* obj );
	// Set a flag that helps the user of the proxy realize that even though IsResident()
	// may be true, the object is just a temporary placeholder.
	void SetTemporary( bool isTemporary );
	bool IsTemporary() const;
	void OnSelected();

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
