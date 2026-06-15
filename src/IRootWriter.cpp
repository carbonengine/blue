// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "IRootWriter.h"
#include "IBlueObjectProxy.h"

#include "BlueResFile.h"

IRootWriter::IRootWriter() :
	m_skipDefaults( true )
{
}

void IRootWriter::WriteMembers( const IRoot &instance, IRoot* defaultInstance )
{
	IBlueObjectProxyPtr proxy( BlueCastPtr( (IRoot*)&instance ) );
	if( proxy )
	{
		// Ensure the object behind the proxy is resident
		proxy->GetObject( );
	}

    const Be::ClassInfo* classInfo = instance.ClassType();

    // Create an instance to use for checking default values.  Don't call Initialize because the defaultInstance is
    // only used for member value comparison and will be destroyed directly after write.
    if( defaultInstance == NULL )
    {
        BeClasses->CreateInstance( *classInfo->mClassId, GetIRootIID(), (void**)&defaultInstance );
    }
    else
    {
        defaultInstance->Lock();
    }


    BlueMemberIterator it( (IRoot*)&instance );

    for( ; ; it.Next() )
    {
		if( m_skipDefaults )
		{
			it.SkipEquals( defaultInstance );
		}

        if( it.Eof() )
        {
            break;
        }

        Be::Var* const var = it.Var();
        const Be::VarEntry * const entry = it.Entry();
        const Be::ClassInfo * const type = it.Type();

        Be::Var* const defaultVar = it.Var( defaultInstance );
        if(entry->mType == Be::BINARYBLOCK)
		{
			/*
				Support for generic binary blocks. Forward the responsibility of decrypting those to the instance if 
				it implements the correct interface for me to send the data for processing.
			*/
			ICustomPersistPtr customPersist;				
			if( ((IRoot*)&instance)->QueryInterface( GetICustomPersistIID(), (void**)&customPersist, BEQI_SILENT ) )
			{
				WriteBinaryBlock(customPersist, entry->mName);
			}
			continue;
		}
		else
		{
			WriteMemberName( entry->mName );
		}
        

        bool ok = true;
        switch(entry->mType)
        {
        case Be::LONG:
            WriteInt32( var->mLong );
            break;
		case Be::ULONG:
			WriteUInt32( var->mULong );
			break;
        case Be::BYTE:
            WriteInt8(var->mByte);
            break;

        case Be::SHORT:
            WriteInt16(var->mShort);
            break;

        case Be::FLOAT:
            WriteFloat(var->mFloat);
            break;

		case Be::FLOATARRAY:
			WriteFloatArray( &var->mFloat, entry->GetFloatArraySize() );
			break;

        case Be::DOUBLE:
            WriteDouble(var->mDouble);
            break;

        case Be::BOOL:
            WriteInt8(var->mBool);
            break;

        case Be::IROOT:
            {
				IListPtr list = BlueCastPtr( reinterpret_cast<IRoot*>( var ) );
                if( list )
                {
					WriteList( list );
                }
                else
                {
					IBlueDictPtr dict = BlueCastPtr( reinterpret_cast<IRoot*>( var ) );
					if( dict )
					{
						WriteDict( dict );
					}
					else
					{
						IBlueStructureListPtr structureList = BlueCastPtr( reinterpret_cast<IRoot*>( var ) );
						if( structureList )
						{
#if BLUE_WITH_PYTHON
							WriteStructureList( structureList );
#endif

						}
						else
						{
							WriteIRoot( *reinterpret_cast<IRoot*>( var ), reinterpret_cast<IRoot*>( defaultVar ) );
						}
					}
                }
            }
            break;

		case Be::IROOTWEAKREF:
			{
				BlueWeakRefBase* weakRef = reinterpret_cast<BlueWeakRefBase*>( var );
				WriteIRoot( *weakRef );
			}
			break;

        case Be::IROOTPTR:
			WriteIRoot( var->mIRootPtr );
            break;

        case Be::CHARARRAY:
            ok = false; // WriteChar((const char*)var);
            break;

        case Be::CSTRING:
        case Be::REFERENCE:
            WriteChar(var->mCharPtr);
            break;

		case Be::STDSTRING:
			{
				const std::string &s = *reinterpret_cast<const std::string*>(var);
				WriteChar( s.c_str() );
			}
			break;

		case Be::STDWSTRING:
			{
				const std::wstring& s = *reinterpret_cast<const std::wstring*>(var);
				WriteWChar( s.c_str() );
			}
			break;

        case Be::WCSTRING:
        case Be::WREFERENCE:
            WriteWChar(var->mWCharPtr);
            break;

        case Be::INT64:
            WriteInt64(var->mInt64);
            break;

		case Be::UINT64:
			WriteUInt64( var->mUInt64 );
			break;

        case Be::PYOBJECTPTR:
            ok = false; //WritePythonObject(var->mPyObject);
            break;

		case Be::SHAREDSTRING:
			{
				const BlueSharedString &s = *reinterpret_cast<const BlueSharedString*>(var);
				WriteChar( s.c_str() );
			}
			break;

		case Be::SHAREDSTRINGW: 
			{
				const BlueSharedStringW& s = *reinterpret_cast<const BlueSharedStringW*>( var );
				WriteWChar( s.c_str() );
			}
			break;

		default:
            CCP_LOGERR( "Unknown blue type in member %s", it.Entry()->mName );

            ok = false;
            break;
        }

        if (!ok)
        {
            CCP_LOGERR( "Writer writing member '%s' of class %s.%s", entry->mName, type->mClassId->GetModule(), type->mClassId->GetName() );
            WriteChar( "Error" );
            return;
        }
    }
    defaultInstance->Unlock();
}

Be::Result<std::string> IRootWriter::WriteObjectToFile( const IRoot* root, std::wstring fileName )
{
    m_fileName = fileName;
    
    ResFilePtr file;
	file.CreateInstance();
    if( !file->CreateW( fileName.c_str() ) )
    {
        return Be::Result<std::string>( "Couldn't create file" );
    }

    return WriteObjectToStream( root, file );
}

void IRootWriter::WriteList( IListPtr list )
{
	ssize_t n = list->GetSize();
	WriteVectorBegin( n );
	for( ssize_t i = 0; i < n; ++i )
	{
		IRoot* p = list->GetAt( i );
		WriteIRoot( p );
	}
	WriteVectorEnd( n );
}

void IRootWriter::WriteDict( IBlueDictPtr dict )
{
	size_t n = dict->GetLength();
	WriteVectorBegin( n );
	for( size_t i = 0; i < n; ++i )
	{
		const char* key = dict->GetKey( i );
		IRoot* p = dict->Subscript( key );
		WriteChar( key );
		WriteIRoot( p );
	}
	WriteVectorEnd( n );
}
