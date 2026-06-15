// Copyright © 2015 CCP ehf.

#pragma once
#ifndef SimpleJson_h
#define SimpleJson_h

#include <sstream>

// This class simplifies the construction of a json represention of a simple object.
class SimpleJson
{
public:
	SimpleJson();

	enum QuotesOrNot {
		NO_QUOTES,
		WITH_QUOTES
	};

	template <typename T> void set( const char* field, const T& value, QuotesOrNot quoted = NO_QUOTES )
	{
		if( !m_isFirst )
		{
			m_output << ", ";
		}
		m_output << "\"" << field << "\": ";

		if( quoted == WITH_QUOTES )
		{
			m_output << "\"";
		}
		m_output << value;
		if( quoted == WITH_QUOTES )
		{
			m_output << "\"";
		}
		m_isFirst = false;
	}

	void set( const char* field, const char* value )
	{
		set( field, value, WITH_QUOTES );
	}

	void set( const char* field, const std::string& value )
	{
		set( field, value, WITH_QUOTES );
	}

	// Returns a json representation of any fields currently set on the object.
	std::string str();

protected:
	std::stringstream m_output;
	bool m_isFirst;
	bool m_isDone;

};


#endif // SimpleJson_h
