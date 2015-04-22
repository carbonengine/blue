////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Filipp Pavlov
// Created:		April 2015
// Copyright:	CCP 2015
//

#include "StdAfx.h"
#include "BlueNetworkStream.h"

BLUE_DEFINE( BlueNetworkStream );

const Be::ClassInfo* BlueNetworkStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueNetworkStream, "" )
		MAP_INTERFACE( IBlueStream )

		MAP_METHOD_AND_WRAP( 
			"__init__", 
			Open, 
			"Creates a network stream. Starts streaming in data from provided URL into memory immediately.\n"
			"Arguments:\n"
			"url - well-formed network url to a resourse" )
		MAP_PROPERTY_READONLY( 
			"size", 
			GetSize, 
			"Size of the file. May block the thread until size is available" );
		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS( 
			"read", 
			ReadData, 
			1, 
			"Reads data from the stream and returns it as a string. Blocks until the data is downloaded.\n"
			"Arguments:\n"
			"size - (optional) size of the data to read in bytes; if no size is provided the function reads entire file" );
		MAP_METHOD_AND_WRAP( 
			"close", 
			Close, 
			"Closes the file and aborts the transfer" );
		MAP_METHOD_AND_WRAP( 
			"tell", 
			Tell, 
			"Returns current read position in the stream" );
	EXPOSURE_END()
}