////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//
// This was moved over from BlueResFile.cpp

#pragma once
#ifndef Stuffer_h
#define Stuffer_h

#include "EmbedFs.h"

#include <string>
#include <set>
#include <map>

extern class Stuffer *BeStuffer; //where we get our stuffed stuff

// StufferNodes are used to build a hierarchy of directories and files
// in all the stuff files. This allows blue.os.listdir to query contents
// of directories.
class StufferNode
{
public:
	// Insert an item under this node. If path contains a '/' it is broken
	// down, the part before the first slash added as a child and rest
	// added to the child node.
	void Insert( const std::string& path );

	// Searches for the path in this node.
	StufferNode* Find( const std::string& path );

	void AddChildrenToSet( std::set<std::wstring>& dst );

	std::map<std::string,StufferNode> m_children;
	std::string m_name;
};

// The stuffer manages lookups into stuff files. 
class Stuffer
{
	struct HashTarget;
public:

	Stuffer();
	~Stuffer();

	static void Startup();
	static void Shutdown();
	static void GetStufferDirectoryContents( const char* dir, std::set<std::wstring>& dst );

	// Returns file attributes for path (0 if it doesn't exist)
	static unsigned int GetStufferAttributes( const char* path );

	// Adds all stuff files from the given folder
	void AddFilesFromFolder( const std::wstring& root );

	EmbedFs::HashValue* BSearchHashValue(const HashTarget &target);
	bool HasData(const wchar_t *filename);
	bool LockData(const wchar_t* filename, void** data, int *handle, size_t* size);
	bool UnlockData(void* data, int handle);

	void GetDirectoryContents( const std::string& path, std::set<std::wstring>& dst );
	unsigned int GetAttributes( const std::string& path );

	bool GetStream( const wchar_t* filename, IBlueStream** stream );

private:

	//a target, used when looking up in the hash.
	struct HashTarget {
		HashTarget(const wchar_t *n);
		HashTarget(const char *n);
	
		unsigned long crc;
		std::string fname;
	};

	typedef TrackableStdVector<EmbedFs*> Embeds;
	Embeds mEmbeds;
	
	EmbedFs::HashValue* mHashes;
	unsigned mNumHashes;

	StufferNode m_root;
};

#endif // Stuffer_h
