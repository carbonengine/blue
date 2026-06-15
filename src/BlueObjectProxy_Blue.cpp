// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "IBlueOS.h"
#include "BlueObjectProxy.h"

BLUE_DEFINE_INTERFACE( IBlueObjectProxy );

BLUE_DEFINE( BlueObjectProxy );

const Be::ClassInfo* BlueObjectProxy::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueObjectProxy, "" )
		MAP_INTERFACE( BlueObjectProxy )
		MAP_INTERFACE( IBlueObjectProxy )
		
		MAP_PROPERTY
		(
			"object",
			GetObject,
			SetObject,
			"Get the blue object this proxy represents"
		)

		MAP_ATTRIBUTE
		(
			"object",
			m_object,
			"This is only for persisting the object behind the proxy.",
			Be::PERSISTONLY
		)

		MAP_ATTRIBUTE
		(
			"builder",
			m_builder,
			"The builder for creating the object",
			Be::READWRITE
		)
		MAP_ATTRIBUTE
		(
			"objectMarker",
			m_objectMarker,
			"Object marker passed to the builder to identify this object",
			Be::READWRITE
		)

		MAP_PROPERTY
		(
			"temporary",
			IsTemporary,
			SetTemporary,
			"True/False: Hint for the LOD handler that the object in the proxy is just a temporary placeholder."
		)

		MAP_PROPERTY_READONLY
		(
			"isResident",
			IsResident,
			"is the object resident"
		)


		MAP_METHOD_AND_WRAP( "Freeze", Freeze, "Prevents unloading of the object")
		MAP_METHOD_AND_WRAP( 
			"SetObjectFromBuilder", 
			SetObjectFromBuilder, 
			"Replaces the object inside the proxy without any further notifications, nor will it unlink the builder.\n"
			":param obj: new object"
			)
	EXPOSURE_END()
}
