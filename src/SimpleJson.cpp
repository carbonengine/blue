////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		June 2015
// Copyright:	CCP 2015
//

#include "StdAfx.h"
#include "SimpleJson.h"

SimpleJson::SimpleJson() : m_isFirst( true ), m_isDone( false )
{
	m_output << "{";
}

std::string SimpleJson::str()
{
	if( !m_isDone )
	{
		m_output << "}";
		m_isDone = true;
	}
	return m_output.str();
}
