// Copyright © 2017 CCP ehf.

#pragma once

#include "IBlueObjectMetadata.h"

BLUE_CLASS( BlueObjectMetadata ): public IBlueObjectMetadata, public IWeakRef
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectMetadata( IRoot* lockobj = nullptr );
	~BlueObjectMetadata();

	const Metadata* GetMetadata( IWeakObject* owner ) const override;

	void Set( IWeakObject* owner, const char* key, const char* value ) override;
	const char* Get( IWeakObject* owner, const char* key, const char* defaultValue ) const override;
	BlueStdResult Delete( IWeakObject* owner, const char* key ) override;
	BlueStdResult DeleteObject( IWeakObject* owner ) override;

	BlueStdResult Index( IWeakObject* owner, const char* key, const char*& value ) const;
	BlueStdResult GetKeys( IWeakObject* owner, std::vector<std::string>& keys ) const;
	BlueStdResult GetItems( IWeakObject* owner, std::map<std::string, std::string>& items ) const;

	BlueStdResult CopyShallow( IWeakObject* source, IWeakObject* target ) override;
	BlueStdResult CopyDeep( IWeakObject* source, IWeakObject* target ) override;
private:
	struct DataTable
	{
		DataTable();

		Metadata mapping;
	};

	void WeakRefNotify( IWeakObject* weak ) override;

	TrackableStdUnorderedMap<IWeakObject*, DataTable*> m_metadata;
};

TYPEDEF_BLUECLASS( BlueObjectMetadata );

BLUEIMPORT void PostCopyMetadata( IRoot* source, IRoot** dest, ICopier* copier, void* context );

#if BLUE_WITH_PYTHON

PyObject* PyCopyWithMetadata( PyObject* src );

#endif
