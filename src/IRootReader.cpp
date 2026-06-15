// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "IRootReader.h"
#include "IBluePersist.h"
#include <BlueStatistics.h>

BLUE_DEFINE_INTERFACE( IRootReader );

// Lookup table for property handlers. Note that we can't use an anonymous namespace here
// as the PropertyHandlerTableSetup class needs to be a friend of the IRootReaderBase, and
// that friend declaration won't work if we put this in an anonymous namespace.
typedef void (IRootReaderBase::*HandlePropertyHelper_t)( Be::Var* var, const Be::VarEntry* entry );
static HandlePropertyHelper_t s_propertyHandlers[Be::VARTYPE_MAX + 1];

class PropertyHandlerTableSetup
{
public:
	PropertyHandlerTableSetup()
	{
		memset( s_propertyHandlers, 0, sizeof( s_propertyHandlers ) );
		s_propertyHandlers[Be::LONG] =			&IRootReaderBase::HandlePropertyLong;
		s_propertyHandlers[Be::ULONG] =			&IRootReaderBase::HandlePropertyULong;
		s_propertyHandlers[Be::FLOAT] =			&IRootReaderBase::HandlePropertyFloat;
		s_propertyHandlers[Be::DOUBLE] =		&IRootReaderBase::HandlePropertyDouble;
		s_propertyHandlers[Be::BOOL] =			&IRootReaderBase::HandlePropertyBool;
		s_propertyHandlers[Be::IROOT] =			&IRootReaderBase::HandlePropertyIRoot;
		s_propertyHandlers[Be::IROOTPTR] =		&IRootReaderBase::HandlePropertyIRootPtr;
		s_propertyHandlers[Be::IROOTWEAKREF] =	&IRootReaderBase::HandlePropertyIRootWeakRef;
		s_propertyHandlers[Be::CSTRING] =		&IRootReaderBase::HandlePropertyCString;
		s_propertyHandlers[Be::INT64] =			&IRootReaderBase::HandlePropertyInt64;
		s_propertyHandlers[Be::UINT64] =		&IRootReaderBase::HandlePropertyUInt64;
		s_propertyHandlers[Be::REFERENCE] =		&IRootReaderBase::HandlePropertyCString;
		s_propertyHandlers[Be::WCSTRING] =		&IRootReaderBase::HandlePropertyWCString;
		s_propertyHandlers[Be::WREFERENCE] =	&IRootReaderBase::HandlePropertyWCString;
		s_propertyHandlers[Be::FLOATARRAY] =	&IRootReaderBase::HandlePropertyFloatArray;
		s_propertyHandlers[Be::STDSTRING] =		&IRootReaderBase::HandlePropertyStdString;
		s_propertyHandlers[Be::STDWSTRING] =	&IRootReaderBase::HandlePropertyStdWString;
		s_propertyHandlers[Be::BYTE] =			&IRootReaderBase::HandlePropertyByte;
		s_propertyHandlers[Be::SHORT] =			&IRootReaderBase::HandlePropertyShort;
		s_propertyHandlers[Be::SHAREDSTRING] =	&IRootReaderBase::HandlePropertySharedString;
		s_propertyHandlers[Be::SHAREDSTRINGW] = &IRootReaderBase::HandlePropertySharedStringW;
	}
};

static PropertyHandlerTableSetup s_propertyHandlersSetup;

IRootReaderBase::IRootReaderBase()
{
}

IRootReaderBase::~IRootReaderBase()
{
}

void IRootReaderBase::HandleAttribute( const char* attributeName, IRoot* instance, INotify* notify )
{
	const Be::ClassInfo* ci = instance->ClassType();

	ssize_t offs = 0;
	const Be::VarEntry* entry = FindEntry( attributeName, ci, offs );

	if( !entry )
	{
		std::string msg = "Invalid attribute: ";
		msg += attributeName;
		throw InvalidAttributeException( msg.c_str() );
	}
	Be::Var* var = (Be::Var*)((uintptr_t)instance + entry->mOffset + offs);

	if( entry->mType > Be::VARTYPE_MAX )
	{
		std::string msg = "Invalid attribute type: ";
		msg += attributeName;
		throw IRootReaderException( msg.c_str() );
	}
	else if( entry->mType == Be::BINARYBLOCK )
	{
		/*
		Support for generic binary blocks. Forward the responsibility of decrypting those to the instance if 
		it implements the correct interface for me to send the data for processing.
		*/
		ICustomPersistPtr customPersist;			
		if( instance->QueryInterface( GetICustomPersistIID(), (void**)&customPersist, BEQI_SILENT ) )
		{
			ReadBinaryBlock(customPersist, attributeName);
		}
	}
	else
	{
		HandlePropertyHelper_t func = s_propertyHandlers[entry->mType];

		if( !func )
		{
			std::string msg = "Invalid attribute type: ";
			msg += attributeName;
			throw IRootReaderException( msg.c_str() );
		}

		// Call the property handler
		try
		{
			(this->*func)( var, entry );
		}
		catch( const IRootReaderException& exc )
		{
			std::string msg = attributeName;
			msg += ": ";
			msg += exc.what();
			throw IRootReaderException( msg.c_str() );
		}
	}

	if( notify && (entry->mEditFlags & Be::NOTIFY) )
	{
		notify->OnModified( var );
	}
}

void IRootReaderBase::HandlePropertyLong( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mLong );
}

void IRootReaderBase::HandlePropertyULong( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mULong );
}

void IRootReaderBase::HandlePropertyByte( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mByte );
}

void IRootReaderBase::HandlePropertyShort( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mShort );
}

void IRootReaderBase::HandlePropertyFloat( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mFloat );
}

void IRootReaderBase::HandlePropertyFloatArray( Be::Var* var, const Be::VarEntry* entry )
{
	ReadFloatArray( &var->mFloat, entry->GetFloatArraySize() );
}

void IRootReaderBase::HandlePropertyDouble( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mDouble );
}

void IRootReaderBase::HandlePropertyBool( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mBool );;
}

void IRootReaderBase::HandlePropertyIRoot( Be::Var* var, const Be::VarEntry* entry )
{
	IListPtr list( BlueCastPtr( reinterpret_cast<IRoot*>( var ) ) );
	if( list )
	{
		ReadList( list );
	}
	else
	{
		IBlueDictPtr dict( BlueCastPtr( reinterpret_cast<IRoot*>( var ) ) );
		if( dict )
		{
			ReadDict( dict );
		}
		else
		{
			IBlueStructureListPtr structureList( BlueCastPtr( reinterpret_cast<IRoot*>( var ) ) );
			if( structureList )
			{
				ReadStructureList( structureList );
			}
			else
			{
				ReadIRoot( *reinterpret_cast<IRoot*>( var ) );
			}
		}
	}
}

void IRootReaderBase::HandlePropertyIRootPtr( Be::Var* var, const Be::VarEntry* entry )
{
	// First, we must check to see if there is an object already in the slot and remove it
	// properly 
	if( var->mIRootPtr )
	{
		var->mIRootPtr->Unlock();
	}

	IRoot* obj = ReadIRootClass();
	if( obj )
	{
		// Query interface will increase the ref count, which is what we want
		obj->QueryInterface( entry->mIID ? *entry->mIID : GetIRootIID(), (void**)&var->mIRootPtr, BEQI_SILENT );
		if( var->mIRootPtr == nullptr )
		{
			throw IRootReaderException( "Incorrect type for member" );
		}
		obj->Unlock();
	}
	else
	{
		var->mIRootPtr = nullptr;
	}
}

void IRootReaderBase::HandlePropertyIRootWeakRef( Be::Var* var, const Be::VarEntry* entry )
{
	BlueWeakRefBase* weakRef = reinterpret_cast<BlueWeakRefBase*>( var );

	IRoot* obj = ReadIRootClass();
	if( obj )
	{
		IRootPtr typedObj;

		// Typed assignment
		obj->QueryInterface( entry->mIID ? *entry->mIID : GetIRootIID(), (void**)&typedObj, BEQI_SILENT );

		if( typedObj == nullptr )
		{
			throw IRootReaderException( "Incorrect type for member" );
		}

		*weakRef = obj;

		// Note that we must release the reference we got from reading the object in.
		// As this is a weak reference there would be a risk of the object dying here,
		// but presumably there is another strong reference somewhere else in the file
		// and the reference map ensures it stays alive until the whole structure has
		// been set up.
		obj->Unlock();
	}
	else
	{
		*weakRef = nullptr;
	}
}

void IRootReaderBase::HandlePropertyCString( Be::Var* var, const Be::VarEntry* entry )
{
	var->mCharPtr = CCP_STRDUP( __FUNCTION__, ReadString() );
}

void IRootReaderBase::HandlePropertyStdString( Be::Var* var, const Be::VarEntry* entry )
{
	std::string &s = *reinterpret_cast<std::string*>(var);
	const char* charPtr = ReadString(); // We lose any /0 characters that are not actually string terminators here
	s.assign( charPtr );
}

void IRootReaderBase::HandlePropertyStdWString( Be::Var* var, const Be::VarEntry* entry )
{
	std::wstring& s = *reinterpret_cast<std::wstring*>(var);
	const wchar_t* wcharPtr = ReadWString();
	if( wcharPtr )
	{
		s.assign( wcharPtr );
	}
}

void IRootReaderBase::HandlePropertyWCString( Be::Var* var, const Be::VarEntry* entry )
{
	var->mWCharPtr = CCP_WSTRDUP( __FUNCTION__, ReadWString() );
}

void IRootReaderBase::HandlePropertyInt64( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mInt64 );
}

void IRootReaderBase::HandlePropertyUInt64( Be::Var* var, const Be::VarEntry* entry )
{
	ReadValue( var->mUInt64 );
}

void IRootReaderBase::HandlePropertyBinaryBlock( Be::Var* var, const Be::VarEntry* entry )
{

}

void IRootReaderBase::HandlePropertySharedString( Be::Var* var, const Be::VarEntry* entry )
{
	BlueSharedString &s = *reinterpret_cast<BlueSharedString*>(var);
	const char* charPtr = ReadString(); // We lose any /0 characters that are not actually string terminators here
	s = BlueSharedString( charPtr );
}

void IRootReaderBase::HandlePropertySharedStringW( Be::Var* var, const Be::VarEntry* entry )
{
	BlueSharedStringW& s = *reinterpret_cast<BlueSharedStringW*>( var );
	const wchar_t* charPtr = ReadWString(); // We lose any /0 characters that are not actually string terminators here
	s = BlueSharedStringW( charPtr );
}

const Be::VarEntry* IRootReaderBase::FindEntry( const char* name, const Be::ClassInfo* type, ssize_t& offs )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Loop over all entries - this double loop covers chaining
	for (; type; offs += type->mOffsetToParent, type = type->mParentClassInfo)
	{
		for (const Be::VarEntry* entry = type->mMemberTable; entry->mName; entry++)
		{
			if( (entry->mEditFlags & Be::PERSIST) && (strcmp(entry->mName, name) == 0) )
			{
				return entry;
			}
		}
	}

	return NULL;
}
