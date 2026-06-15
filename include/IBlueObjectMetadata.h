// Copyright © 2017 CCP ehf.

#pragma once

#define BLUE_OBJECT_METADATA_KEY "__bluemetadata__"

BLUE_INTERFACE( IBlueObjectMetadata ): public IRoot
{
	typedef TrackableStdUnorderedMap<std::string, std::string> Metadata;

	virtual const Metadata* GetMetadata( IWeakObject* owner ) const = 0;
	virtual void Set( IWeakObject* owner, const char* key, const char* value ) = 0;
	virtual const char* Get( IWeakObject* owner, const char* key, const char* defaultValue ) const = 0;
	virtual BlueStdResult Delete( IWeakObject* owner, const char* key ) = 0;
	virtual BlueStdResult DeleteObject( IWeakObject* owner ) = 0;
	virtual BlueStdResult CopyShallow( IWeakObject* source, IWeakObject* target ) = 0;
	virtual BlueStdResult CopyDeep( IWeakObject* source, IWeakObject* target ) = 0;
};

extern BLUEIMPORT IBlueObjectMetadata* BeObjectMetadata;
