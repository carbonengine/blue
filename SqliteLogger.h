
////////////////////////////////////////////////////////////
//
//    Creator:   Oddur Magnusson
//    Created:   November 2011
//    Copyright: CCP 2011
//

#pragma once
#ifndef CCP_SQLLITELOGGER_H
#define CCP_SQLLITELOGGER_H

#if CCP_ENABLE_SQLITE_LOGGING

#include "Logger\logger.h"
#include "include\Blue.h"
#include "include\IBlueOS.h"
#include <stackless\Modules\_sqlite\sqlite3.h>

namespace CCP
{
	// Sets up network logging which will forward messages in to the network via UDP to the address specified
	bool SetupSqliteLogging(const char* dbName, CCP::LogType threshold );

	// CCP Log callback which will forward to the sqlilite logger instance
	void ForwardToSqlite( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message );

	// Class the encapsulates the logging logic
	class SqliteLogger : public IBlueEvents
	{
	private:
		sqlite3 *m_db;
		sqlite3_stmt *m_insertLogStatement;
		int m_numStatementsInTransaction;
		Be::Time m_lastCommitTime;

	public:
		bool Init( const char* dnName );
		void Stop();

		// Logging call back which will forward logging messages. Will not do anything if SetupNetworkLogging has not been called
		void LogToSqlite( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message );

		// commits the pending statements to disk
		void Commit();

		// ticking callback to persist logs to disk
		virtual void OnTick( Be::Time realTime, Be::Time simTime, void* cookie );

	};
}

#endif

#endif