#pragma once

#include "Include/IBlueObjectMetadata.h"

BLUE_CLASS( BlueObjectMetadata ): public IBlueObjectMetadata, public IWeakRef
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectMetadata( IRoot* lockobj = nullptr );
	~BlueObjectMetadata();

	const Metadata* GetMetadata( IWeakObject* owner ) const;

	virtual void Set( IWeakObject* owner, const char* key, const char* value );
	virtual const char* Get( IWeakObject* owner, const char* key, const char* defaultValue ) const;
	virtual BlueStdResult Delete( IWeakObject* owner, const char* key );
	virtual BlueStdResult DeleteObject( IWeakObject* owner );

	BlueStdResult Index( IWeakObject* owner, const char* key, const char*& value ) const;
	BlueStdResult GetKeys( IWeakObject* owner, std::vector<std::string>& keys ) const;
	BlueStdResult GetItems( IWeakObject* owner, std::map<std::string, std::string>& items ) const;
private:
	struct DataTable
	{
		DataTable();

		Metadata mapping;
	};

	virtual void WeakRefNotify( IWeakObject* weak );

	TrackableStdUnorderedMap<IWeakObject*, DataTable*> m_metadata;
};

TYPEDEF_BLUECLASS( BlueObjectMetadata );