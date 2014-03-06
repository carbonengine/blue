////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//
// This was moved over from BlueResFile.cpp
// 

#include "StdAfx.h"


#include "Stuffer.h"
#include "include/IBluePaths.h"
#include "include/BlueFileUtil.h"
#include <zlib.h>

Stuffer *BeStuffer = 0;

#if STUFFER_ENABLED

static CcpLogChannel_t s_chFile = CCP_LOG_DEFINE_CHANNEL( "File" );

// TODO: replace with a global g_blueVerbose flag
static bool s_verbose = false;

Stuffer::HashTarget::HashTarget( const wchar_t *n ) :
	fname( CW2A( EmbedFs::ToLower( n ).c_str() ) )
{
	crc = crc32( 1, (const unsigned char*)fname.c_str(), (int)fname.size() );
}

Stuffer::HashTarget::HashTarget( const char *n ) :
	fname( EmbedFs::ToLower( n ) )
{
	crc = crc32( 1, (const unsigned char*)fname.c_str(), (int)fname.size() );
}



void Stuffer::Startup()
{
	CCP_LOG( "Stuffer::Startup" );

	if (!BeStuffer)
	{
		BeStuffer = CCP_NEW( "ResFile/sStuffer" ) Stuffer();
	}
}


void Stuffer::Shutdown()
{
	// Can't really delete the stuffer here, as there may be
	// ResFile instances in static destructors that will want
	// to talk to the stuffer after this shutdown.
}



void Stuffer::GetStufferDirectoryContents( const char* dir, std::set<std::wstring>& dst )
{
	if( !BeStuffer )
	{
		return;
	}

	std::string normDir;
	if( !NormalizeResPath( dir, normDir ) )
	{
		return;
	}

	// Stuff files are only used for res: paths currently
	// NormalizeResPath above ensured it starts with res:
	// remove that from the start of the string.
	normDir.erase( 0, 4 );

	if( normDir[0] == '/' )
	{
		normDir.erase( 0, 1 );
	}

	BeStuffer->GetDirectoryContents( normDir, dst );
}


unsigned int Stuffer::GetStufferAttributes( const char* path )
{
	if( !BeStuffer )
	{
		return false;
	}

	std::string normDir;
	if( !NormalizeResPath( path, normDir ) )
	{
		return false;
	}

	// Stuff files are only used for res: paths currently
	// NormalizeResPath above ensured it starts with res:
	// remove that from the start of the string.
	normDir.erase( 0, 4 );

	if( normDir[0] == '/' )
	{
		normDir.erase( 0, 1 );
	}

	unsigned int attrs = BeStuffer->GetAttributes( normDir );
	return attrs;
}


Stuffer::Stuffer() :
	mEmbeds( "Stuffer/mEmbeds" )
{
	mNumHashes= 0;
	mHashes = NULL;
}

void Stuffer::AddFilesFromFolder( const std::wstring& root )
{
	CCP_LOG_CH( s_chFile, "Scanning %S for stuff files", root.c_str() );

	// TODO: Do we want to extend this to allow stuff files to be added later?
	CCP_ASSERT( !mHashes );

	std::wstring filename = root + L"*.stuff";

	std::vector<std::wstring> stuffFilenames;

#ifdef _WIN32

	WIN32_FIND_DATAW fd;
	
	HANDLE find = FindFirstFileW(filename.c_str(), &fd);
	if( find == INVALID_HANDLE_VALUE )
	{
		// No stuff files found
		return;
	}

	do
	{
		filename = root + fd.cFileName;
		if( s_verbose )
		{
			CCP_LOG_CH( s_chFile, "Stuff file: %S\n", filename.c_str() );
		}
		stuffFilenames.push_back( filename );
	} while( FindNextFileW(find, &fd) );

	FindClose( find );
#endif

	//filenames are found.  create the EmbedFs dudes for them, and count the files.
	for( size_t i = 0; i<stuffFilenames.size(); i++ )
	{
		EmbedFs* embed = new EmbedFs(stuffFilenames[i].c_str());
		unsigned nFiles = embed->NumFiles();
		if( nFiles )
		{
			mNumHashes += nFiles;
			mEmbeds.push_back(embed);
		}
		else
		{
			delete embed;
		}
	}

	if( mNumHashes == 0 )
	{
		CCP_LOGERR_CH( s_chFile, "No files found in stuff files" );
		return;
	}

	if( mEmbeds.empty() )
	{
		CCP_LOGERR_CH( s_chFile, "No valid stuff files found" );
		return;
	}

	bool failed = false;

	//Create hash array and load the hash values into the table
	mHashes = new EmbedFs::HashValue[mNumHashes];
	if( !mHashes )
	{
		CCP_LOGERR_CH( s_chFile, "Couldn't allocate memory for %d file entries (corrupt stuff files?)", mNumHashes );
		mNumHashes = 0;
		return;
	}

	unsigned index = 0;
	for( unsigned i = 0; i < mEmbeds.size(); i++ )
	{
		if (!mEmbeds[i]->LoadHashValues(mHashes + index, i))
		{
			//oh, a failure.  Oh well
			failed = true;
			break;
		}
		index += mEmbeds[i]->NumFiles();
	}

	if( failed )
	{
		delete [] mHashes;
		mHashes = 0;
		while (mEmbeds.size())
		{
			delete mEmbeds.back();
			mEmbeds.pop_back();
		}

		return;
	}

	//sort it for duplicate test and our BSearch
	std::sort(mHashes, mHashes+mNumHashes);

	EmbedFs::HashValue* p = mHashes;
	EmbedFs::HashValue* end = mHashes + mNumHashes;
	while( p < end )
	{
		m_root.Insert( p->fname.c_str() + 1 );
		++p;
	}
}

Stuffer::~Stuffer()
{
	while(mEmbeds.size()){
		delete mEmbeds.back();
		mEmbeds.pop_back();
	}
	delete[] mHashes;
}

EmbedFs::HashValue* Stuffer::BSearchHashValue(const HashTarget &target)
{
	if (!mNumHashes)
		return 0;
    size_t lo=0, hi=mNumHashes;
	while (hi-lo > 1)
	{
		size_t mid = (lo+hi)/2;
		if (target.crc < mHashes[mid].crc)
			hi = mid;
		else
			lo = mid;
	}
	//lo now points to the last in the line of equal hashes, or something lower
	while( mHashes[lo].crc == target.crc )
	{
		if( strcmp( mHashes[lo].fname.c_str(), target.fname.c_str() ) == 0 )
		{
			return &mHashes[lo];
		}
		if( lo==0 )
		{
			break;
		}
		--lo;
	}
	return 0;
}


bool Stuffer::HasData(const wchar_t* filename)
{
	if( mHashes == NULL )
	{
		return false;
	}
	const EmbedFs::HashValue *r = BSearchHashValue(HashTarget(filename));

	if( s_verbose && r )
	{
		CCP_LOG_CH( s_chFile, "HasData: Found %s in .stuff file %d", filename, r->stuffFile );
	}

	return r != 0;
}


bool Stuffer::LockData(const wchar_t* filename, void** data, int *handle, size_t *size)
{
	
	if( mHashes == NULL )
	{
		*data = NULL;
		*size = 0;
		return false;
	}
	
	EmbedFs::HashValue* hv = BSearchHashValue(HashTarget(filename));
	if( hv == NULL )
	{
		*data = NULL;
		*size = 0;
		return false;
	}
	else
	{
		CCP_ASSERT( hv->size );
		if(	s_verbose )
		{
			CCP_LOG_CH( s_chFile, "LockData: Found %S in .stuff file %d", filename, hv->stuffFile );
		}

		*size = hv->size;
		*data = mEmbeds[hv->stuffFile]->GetFileData(hv->offs, hv->size);
		*handle = hv->stuffFile;
		return true;
	}
}


bool Stuffer::UnlockData(void* data, int handle)
{
	mEmbeds[handle]->ReleaseFileData(data);
	return true;
}

void Stuffer::GetDirectoryContents( const std::string& path, std::set<std::wstring>& dst )
{
	if( path.empty() )
	{
		m_root.AddChildrenToSet( dst );
	}
	else
	{
		StufferNode* node = m_root.Find( path );

		if( node )
		{
			node->AddChildrenToSet( dst );
		}
	}
}

unsigned int Stuffer::GetAttributes( const std::string& path )
{
#ifdef _WIN32
	// Everything in stuff files is read only
	unsigned int attrs = FILE_ATTRIBUTE_READONLY;

	if( path.empty() )
	{
		// Root is a directory
		attrs |= FILE_ATTRIBUTE_DIRECTORY;
	}
	else
	{
		StufferNode* node = m_root.Find( path );

		if( node )
		{
			if( !node->m_children.empty() )
			{
				attrs |= FILE_ATTRIBUTE_DIRECTORY;
			}
		}
		else
		{
			// Wasn't found
			attrs = 0;
		}

	}

	return attrs;
#else
	return 0;
#endif
}

bool Stuffer::GetStream( const wchar_t* filename, IBlueStream** stream )
{
	if( mHashes == NULL )
	{
		return false;
	}

	std::wstring tmp( filename + 4 ); //cut off the "res:" prefix
	NormalizeSlashes( tmp );
	const wchar_t* fn = tmp.c_str();

	const EmbedFs::HashValue *r = BSearchHashValue(HashTarget(fn));

	if( !r )
	{
		return false;
	}

	if( s_verbose && r )
	{
		CCP_LOG_CH( s_chFile, "GetStream: Found %S in .stuff file %d", filename, r->stuffFile );
	}

	return mEmbeds[r->stuffFile]->GetStream( r->offs, r->size, stream );
}


void StufferNode::Insert( const std::string& path )
{
	std::string::size_type s = path.find('/');
	if( s == std::string::npos )
	{
		StufferNode& node = m_children[path];
		node.m_name = path;
	}
	else
	{
		std::string folder = path.substr( 0, s );
		std::string rest = path.substr( s + 1 );

		StufferNode& node = m_children[folder];
		node.m_name = folder;
		node.Insert( rest );
	}
}

StufferNode* StufferNode::Find( const std::string& path )
{
	std::string::size_type s = path.find('/');
	if( s == std::string::npos )
	{
		std::map<std::string,StufferNode>::iterator it = m_children.find( path );
		if( it != m_children.end() )
		{
			// Found it!
			return &it->second;
		}
	}
	else
	{
		std::string folder = path.substr( 0, s );
		std::string rest = path.substr( s + 1 );

		std::map<std::string,StufferNode>::iterator it = m_children.find( folder );
		if( it != m_children.end() )
		{
			// One of our children matches the folder - see if it holds the rest
			StufferNode* result = it->second.Find( rest );
			if( result )
			{
				return result;
			}
		}
	}

	// Not found
	return NULL;
}

void StufferNode::AddChildrenToSet( std::set<std::wstring>& dst )
{
	std::map<std::string,StufferNode>::iterator it;
	for( it = m_children.begin(); it != m_children.end(); ++it )
	{
		std::wstring ws( CA2W( it->second.m_name.c_str() ) );
		dst.insert( ws );
	}
}
#endif
