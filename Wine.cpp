#include "StdAfx.h"
#include "Include/Wine.h"

#ifdef _WIN32
#include <atlbase.h>

namespace Wine
{
	bool IsWine()
	{
		static bool hasCached = false;
		static bool wine = false;
		if( !hasCached )
		{
			HMODULE hMod = GetModuleHandle( "ntdll" );
			wine = GetProcAddress( hMod, "wine_get_version" ) != nullptr;
			hasCached = true;
		}
		return wine;
	}

	const char* GetWineVersion()
	{
		typedef const char* (CDECL *wine_get_version_t)( void );

		static bool hasCached = false;
		static char* wineVersion = "";
		if( !hasCached )
		{
			HMODULE hMod = GetModuleHandle( "ntdll" );
			wine_get_version_t wine_get_version = (wine_get_version_t)GetProcAddress( hMod, "wine_get_version" );

			if( wine_get_version )
			{
				wineVersion = strdup( wine_get_version() );
			}

			hasCached = true;
		}

		return wineVersion;
	}

	const char* GetWineHostOs()
	{
		typedef void (CDECL *wine_get_host_version_t)( const char **sysname, const char **release );

		static bool hasCached = false;
		static char* hostOs = "";
		if( !hasCached )
		{
			HMODULE hMod = GetModuleHandle( "ntdll" );
			wine_get_host_version_t wine_get_host_version = (wine_get_host_version_t)GetProcAddress( hMod, "wine_get_host_version" );

			if( wine_get_host_version )
			{
				const char* sys_name = NULL;
				const char* release_name = NULL;
				wine_get_host_version( &sys_name, &release_name );

				std::string hostOsA = sys_name;
				hostOsA += " ";
				hostOsA += release_name;

				hostOs = strdup( hostOsA.c_str() );
			}

			hasCached = true;
		}

		return hostOs;
	}
}

#else

namespace Wine
{
	bool IsWine() { return false;  }
	const char* GetWineVersion() { return ""; }
	const char* GetWineHostOs() { return ""; }
}

#endif