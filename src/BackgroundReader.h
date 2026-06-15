// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BackgroundReader_h
#define BackgroundReader_h

#include "BlueResManBackgroundCall.h"
BLUE_DECLARE( MemStream );
BLUE_DECLARE_INTERFACE( IBlueStream );

class BackgroundReader : public IBlueResManBackgroundCall
{
public:
	BackgroundReader( const std::wstring& filename );
	virtual ~BackgroundReader();

	// This is called on a background thread
	virtual void Perform();

	Be::Result<std::string> GetResult();

	void TakeContents( IBlueStream** contents );

private:
	std::wstring m_filename;
	MemStreamPtr m_contents;
	Be::Result<std::string> m_result;
};

#endif // BackgroundReader_h
