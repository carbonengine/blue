// Copyright © 2014 CCP ehf.

#pragma once
#ifndef IRootReader_H
#define IRootReader_H

#include <stdio.h>
#include <assert.h>
#include <typeinfo>
#include <vector>
#include <string>

struct IBlueStream;

BLUE_INTERFACE( IRootReader ) : public IRoot
{
    virtual IRoot* ReadFromStream( IBlueStream* stream ) = 0;
	virtual bool ReadForCachingFromStream( IBlueStream* stream ) = 0;
	virtual void SetFileName( const wchar_t* name ) = 0;
	virtual void SetDoInitialize( bool b ) = 0;
	virtual void SetTimeSlice( float t ) = 0;
	
	// Call this after ReadFromStream if it returned a nullptr.
	virtual void GetErrorMessage( std::string& msg ) = 0;
};

class IRootReaderException : public std::exception
{
public:
	IRootReaderException( const char* message ) throw()
		:m_message( message )
	{ 
	}
	virtual ~IRootReaderException() throw()
	{
	}

	virtual const char* what() const throw()
	{
		return m_message.c_str();
	}
private:
	std::string m_message;
};

class InvalidAttributeException : public IRootReaderException
{
public:
	InvalidAttributeException( const char* message ) throw()
		: IRootReaderException( message )
	{ 
	}
};

class IRootReaderBase
{
public:
	IRootReaderBase();
	virtual ~IRootReaderBase();

protected:
	// This base class provides this function to handle a single attribute of 'instance'.
	// The optional 'notify' is called after filling in the attribute. HandleAttribute uses
	// the property handlers listed below to handle the attribute based on its type. Those
	// handlers in turn call the various Read functions listed below. Implementations of those
	// must be provided by subclasses.
	// Using this base class helps ensure that different implementation of readers handle all
	// supported types consistently, making it easier to add new types, and reduces the code
	// needed to implement a new type of reader.
	void HandleAttribute( const char* attributeName, IRoot* instance, INotify* notify );

	// Helper functions for ReadAttribute - called via a lookup table
	void HandlePropertyInt64( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyUInt64( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyWCString( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyStdWString( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyStdString( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyCString( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyIRootPtr( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyIRootWeakRef( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyIRoot( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyBool( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyDouble( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyFloatArray( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyFloat( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyShort( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyByte( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyLong( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyULong( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertyBinaryBlock( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertySharedString( Be::Var* var, const Be::VarEntry* entry );
	void HandlePropertySharedStringW( Be::Var* var, const Be::VarEntry* entry );

	// The property handlers are stored in a static table in the cpp file,
	// need to make this class a friend so it can set up the table.
	friend class PropertyHandlerTableSetup;

	// This function is called by HandleAttribute - can be overridden by subclasses
	virtual const Be::VarEntry* FindEntry( const char* name, const Be::ClassInfo* type, ssize_t& offs );

protected:
	virtual void ReadValue( int64_t& dst ) = 0;
	virtual void ReadValue( uint64_t& dst ) = 0;
	virtual void ReadValue( uint32_t& dst ) = 0;
	virtual void ReadValue( int32_t& dst ) = 0;
	virtual void ReadValue( uint16_t& dst ) = 0;
	virtual void ReadValue( uint8_t& dst ) = 0;
	virtual void ReadValue( bool& dst ) = 0;
	virtual void ReadValue( float& dst ) = 0;
	virtual void ReadValue( double& dst ) = 0;

	virtual const char* ReadString() = 0;
	virtual const wchar_t* ReadWString() = 0;
	virtual void ReadBinaryBlock( ICustomPersist* instance, const char* propertyName ) = 0;

	virtual void ReadFloatArray( float* mFloat, size_t count ) = 0;
	virtual void ReadList( IList* list ) = 0;
	virtual void ReadDict( IBlueDict* dict ) = 0;
	virtual void ReadStructureList( IBlueStructureList* structureList ) = 0;
	virtual void ReadIRoot( IRoot& obj ) = 0;
	virtual IRoot* ReadIRootClass() = 0;

};
#endif