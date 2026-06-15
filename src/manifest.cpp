// Copyright © 2007 CCP ehf.

#include "StdAfx.h"

#include "manifest.h"
#include "crypto.h"
#include "BlueResFile.h"
#include "IBluePaths.h"
#include "errormessage.h"

#include <vector>
#include <string>


constexpr uint32_t MANIFEST_VERSION = 4;
enum ManifestEntryType : uint32_t
{
	TYPE_FILE = 0,
	TYPE_DIRECTIVE = 1
};


static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Manifest" );


class InBuf
{
public:
	InBuf( size_t size )
	{
		m_buf.resize( size );
	}

	Be::Result<std::string> Read( uint32_t& returnValue )
	{
		return Read( &returnValue, sizeof( returnValue ) );
	}

	Be::Result<std::string> Read( std::string& returnValue )
	{
		uint32_t size;
		BE_RETURN_ON_ERROR( Read( size ) );
		std::vector<uint8_t> buffer( size );
		BE_RETURN_ON_ERROR( Read( &buffer[0], size ) );
		returnValue = std::string( std::begin( buffer ), std::end( buffer ) );

		return std::string();
	}

	void* GetData() { return &m_buf[0]; }
	size_t GetPos() const { return m_pos; }

private:
	Be::Result<std::string> Read( void* r, size_t s )
	{
		if( s > m_buf.size() - m_pos )
		{
			return std::string("InBuf::Read: Trying to read too many bytes");
		}

		memcpy( r, &m_buf[m_pos], s );
		m_pos += s;
		return std::string();
	}

	std::vector<uint8_t> m_buf;
	size_t m_pos = 0;
};


struct ManifestEntry_t
{
	ManifestEntry_t( const std::string& name, const std::string& hash ) :
		m_name( name ),
		m_hash( hash )
	{}
	std::string m_name;
	std::string m_hash;
};
using Manifest_t = std::vector<ManifestEntry_t>;


Be::Result<std::string> VerifyManifestEntry( const ManifestEntry_t& m )
{
	IResFilePtr file;
	if( !file.CreateInstance( GetResFileClsid() ) )
	{
		return std::string("VerifyManifestEntry: Could not create IResFilePtr instance");
	}

	if( !file->Open( m.m_name.c_str(), true ) )
	{
		return TranslateErrorMessage( "File not found: ", IDS_VERIFYFAIL_NOTFOUND ) + std::string( m.m_name ) + std::string( "\n" );
	}

	ssize_t fileSize = file->GetSize();
	if( fileSize < 0 )
	{
		return std::string("VerifyManifestEntry: fileSize < 0");
	}

	void *buf;
	if( !file->LockData( &buf, 0 ) )
	{
		return std::string("VerifyManifestEntry: Could not lock tmp stream");
	}
	
	std::string hash;
	BE_RETURN_ON_ERROR( SHA256( buf, fileSize, hash ) );

	if( m.m_hash != hash )
	{
		return std::string("VerifyManifestEntry: Invalid manifest entry hash");
	}

	CCP_LOG_CH( s_ch, "Verification passed: \"%s\"", m.m_name.c_str() );
	return std::string();
}


Be::Result<std::string> UnpackManifest( Manifest_t& m, directives_t& directives, InBuf& ib, AsymmetricCipher& verCipher )
{
	uint32_t version;
	BE_RETURN_ON_ERROR( ib.Read( version ) );

	if( version != MANIFEST_VERSION )
	{
		return "UnpackManifest: invalid manifest version " + std::to_string( version );
	}

	uint32_t entryCount;
	BE_RETURN_ON_ERROR( ib.Read( entryCount ) );

	CCP_LOG_CH( s_ch, "UnpackManifest: Reading manifest file version %d with %d entries", version, entryCount );

	for( uint32_t i = 0; i < entryCount; i++ )
	{
		uint32_t type;
		BE_RETURN_ON_ERROR( ib.Read( type ) );

		switch( type )
		{
		case TYPE_FILE:
		{
			std::string name, hash;
			BE_RETURN_ON_ERROR( ib.Read( name ) );
			BE_RETURN_ON_ERROR( ib.Read( hash ) );
			m.push_back( ManifestEntry_t( name, hash ) );
			break;
		}
		case TYPE_DIRECTIVE:
		{
			std::string name;
			BE_RETURN_ON_ERROR( ib.Read( name ) );
			directives.push_back( name );
			break;
		}
		default:
			return "UnpackManifest: invalid field type " + std::to_string( type );
		}
	}

	std::string timeStamp;
	BE_RETURN_ON_ERROR( ib.Read( timeStamp ) );
	CCP_LOG_CH( s_ch, "Manifest timestamp: %s", timeStamp.c_str() );

	uint32_t isPrivateKey;
	BE_RETURN_ON_ERROR( ib.Read( isPrivateKey ) );
	
	// Load public/private key
	std::string key;
	BE_RETURN_ON_ERROR( ib.Read( key ) );

	bool returnValue;
	if( isPrivateKey )
	{
		BE_RETURN_ON_ERROR( verCipher.LoadPrivateKey( key, "", returnValue ) );
	}
	else
	{
		BE_RETURN_ON_ERROR( verCipher.LoadPublicKey( key, "", returnValue ) );
	}
	if( !returnValue )
	{
		return std::string("UnpackManifest: Invalid key");
	}

	std::string hash, signature;
	BE_RETURN_ON_ERROR( SHA256( ib.GetData(), ib.GetPos(), hash ) ); // Hash everything up until now
	BE_RETURN_ON_ERROR( ib.Read( signature ) ); // Rest of file is the signature

	if( !BeIsSuccess( verCipher.VerifySignature( hash, signature, returnValue ) ) || !returnValue )
	{
		return std::string("UnpackManifest: Invalid manifest signature");
	}

	return std::string();
}


Be::Result<std::string> VerifyManifestFile( const std::string& name, directives_t &directives )
{
	IResFilePtr tmp;	
	if( !tmp.CreateInstance( GetResFileClsid() ) )
	{
		return std::string("VerifyManifestFile: Could not create IResFilePtr instance");
	}

	if( !tmp->Open( name.c_str(), true ) )
	{
		return "VerifyManifestFile: Couldn't open manifest file " + name;
	}

	ssize_t fileSize = tmp->GetSize();
	if( fileSize < 0 )
	{
		return std::string("VerifyManifestFile: fileSize < 0");
	}

	InBuf ib( fileSize );
	if( tmp->Read( ib.GetData(), fileSize ) != fileSize )
	{
		return std::string("VerifyManifestFile: Read problem");
	}

	Manifest_t manifest;
	BE_RETURN_ON_ERROR( UnpackManifest( manifest, directives, ib, *GetSharedAsymmetricCipher() ) );

	std::wstring executablePath = CcpGetAbsolutePath( CcpExecutablePath() );
	bool executableChecked = false;

	for( const ManifestEntry_t& entry : manifest )
	{
		BE_RETURN_ON_ERROR( VerifyManifestEntry( entry ) );

		std::wstring entryName = BePaths->ResolvePathW( std::wstring( CA2W( entry.m_name.c_str() ) ) );
		if( CcpGetAbsolutePath( entryName ) == executablePath )
		{
			executableChecked = true;
		}
	}
	if( !executableChecked && !manifest.empty() )
	{
		return std::string("Executable path not included in manifest file");
	}
    
	CCP_LOG_CH( s_ch, "Successfully verified manifest file %s", name.c_str() );
	return std::string();
}
