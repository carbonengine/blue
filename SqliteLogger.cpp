////////////////////////////////////////////////////////////
//
//    Creator:   Oddur Magnusson
//    Created:   November 2011
//    Copyright: CCP 2011
//

#include "StdAfx.h"

#if CCP_ENABLE_SQLITE_LOGGING

#include "SqliteLogger.h"
#include "include/CcpStatistics.h"
#include <time.h>

namespace CCP
{
	static const char* s_logType2string[ CCP::LOGTYPE_COUNT ] = { "Info ", "Notice ", "Warning ", "Error" };	

	// the sqlitelogger instance if one has been setup
	static SqliteLogger * s_sqliteLogger = NULL;

	// safeguard, if we start logging more than this in the flushperiod, we commit
	const static int MAX_UNCOMMITED = 50000;

	// name of the blueticker job
	static const char* s_tickCookie = "SqliteLogger";

	// Ms to wait until until waiting for a busy database
	static const int SQLITE_BUSY_TIMEOUT = 5;

	static const int STATEMENT_SIZE = 1024*8;

	static const int COMMIT_EVERY_MS = 500 * 10000L;

	// Helper function to execute one of statements with no params
	void executeStatement( sqlite3* db, const char* sql )
	{
		//create the prepared statements we need
		sqlite3_stmt* statement;
		sqlite3_prepare_v2( db, 
			sql,
			STATEMENT_SIZE,
			&statement,
			NULL
			);

		sqlite3_step(statement);

		sqlite3_finalize( statement );
	}

	// Sets up network logging which will forward messages in to the network via UDP to the address specified
	bool SetupSqliteLogging(const char* dbName,  CCP::LogType threshold )
	{
		CCP_STATS_ZONE( __FUNCTION__ );

		s_sqliteLogger = CCP_NEW("SqliteLogger/Logger") SqliteLogger();
		bool ret = s_sqliteLogger->Init(dbName);

		// Instruct the CCP logging system to echo to the debugger output window.
		CCP::RegisterLogEcho( &CCP::ForwardToSqlite, threshold, true, CCP::LOG_ECHO_REQUIRES_PRIVILEGE_CHECK );

		return ret;
	}

	// Closes a sqlogger
	void ShutdownSqliteLogger()
	{
		if (s_sqliteLogger != NULL)
		{
			s_sqliteLogger->Commit();
			CCP_DELETE s_sqliteLogger;
			s_sqliteLogger = NULL;
		}
	}

	// Initialized a sqlogger to the file specified in dbName
	bool SqliteLogger::Init( const char* dbName )
	{
		int dbres = sqlite3_open(dbName, &m_db);
		if (dbres == SQLITE_OK)
		{
			// set a default sleep busy handler with a 5ms sleep
			sqlite3_busy_timeout( m_db, SQLITE_BUSY_TIMEOUT );

			executeStatement( m_db, "create table log (timestamp timestamp, time integer, pid integer, level integer, type string, module text, channel text, message text)" );

			sqlite3_prepare_v2( m_db, 
				"insert into log values (?, ?, ?, ?, ?, ?, ?, ?)",
				STATEMENT_SIZE,
				&m_insertLogStatement,
				NULL
				);

			m_lastCommitTime = BeOS->GetActualTime();
			m_numStatementsInTransaction = 0;
			BeOS->RegisterForTicks( this, (void*)s_tickCookie );

			BeOS->RegisterIndispensableTerminationStep( &CCP::ShutdownSqliteLogger );

			return true;
		}
		return false;
	}

	void SqliteLogger::Stop()
	{
		BeOS->UnregisterForTicks( this, (void*)s_tickCookie );

		Commit();
		sqlite3_finalize( m_insertLogStatement );
		sqlite3_close( m_db );		
	}




	// Logging call back which will forward logging messages. Will not do anything if SetupNetworkLogging has not been called
	void SqliteLogger::LogToSqlite( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message )
	{
		CCP_STATS_ZONE( __FUNCTION__ );

		if ( m_numStatementsInTransaction == 0 )
		{
			executeStatement(m_db, "BEGIN TRANSACTION;");
		}

		sqlite3_reset( m_insertLogStatement );

		time_t rawtime;
		struct tm timeinfo;
		char buffer [80];
		time ( &rawtime );
		localtime_s ( &timeinfo, &rawtime );
		strftime ( buffer, 80, "%Y-%m-%d %H:%M:%S", &timeinfo );

		sqlite3_bind_text( m_insertLogStatement, 1, buffer, -1, NULL );
		sqlite3_bind_int64( m_insertLogStatement, 2, BeOS->GetActualTime() );   // time
		sqlite3_bind_int( m_insertLogStatement, 3, GetCurrentProcessId());   // time
		sqlite3_bind_int( m_insertLogStatement, 4, type );  // LEVEL
		sqlite3_bind_text( m_insertLogStatement, 5, s_logType2string[ (int)type ], -1, NULL );   // TYPE
		sqlite3_bind_text( m_insertLogStatement, 6, logObject.facility, -1, NULL );  // MODULE
		sqlite3_bind_text( m_insertLogStatement, 7, logObject.object, -1, NULL ); // CHN
		sqlite3_bind_text( m_insertLogStatement, 8, message, -1, NULL );   //message

		sqlite3_step(m_insertLogStatement);

		m_numStatementsInTransaction++;

		// time to commit based on number of entries ?
		if(m_numStatementsInTransaction > MAX_UNCOMMITED)
		{
			Commit();
		}
	} 


	void SqliteLogger::Commit()
	{
		CCP_STATS_ZONE( __FUNCTION__ );
		if ( m_numStatementsInTransaction != 0 )
		{
			executeStatement(m_db, "COMMIT TRANSACTION;");
			m_numStatementsInTransaction = 0;
			m_lastCommitTime = BeOS->GetActualTime();
		}
	}


	void SqliteLogger::OnTick( Be::Time realTime, Be::Time simTime, void* cookie )
	{
		CCP_STATS_ZONE( __FUNCTION__ );

		// Time to commit based on time ?
		Be::Time timeSinceLastCommit = BeOS->GetActualTime() - m_lastCommitTime;
		if (timeSinceLastCommit > ( COMMIT_EVERY_MS ))
		{
			Commit();
		}
	}

	void ForwardToSqlite( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message )
	{
		if (s_sqliteLogger != NULL)
		{
			s_sqliteLogger->LogToSqlite( logObject, type, userData, message );
		}
	}

}





PyObject* PyEnableSqliteLogging( PyObject* self, PyObject* args )
{
	char *dbName;
	int threshold = 0;

	if( PyArg_ParseTuple( args, "s|i", &dbName, &threshold ) )
	{
		if( threshold < CCP::LOGTYPE_LOWEST )
		{
			threshold = CCP::LOGTYPE_LOWEST;
		}
		if( threshold > CCP::LOGTYPE_HIGHEST )
		{
			threshold = CCP::LOGTYPE_HIGHEST;
		}


		bool ret = CCP::SetupSqliteLogging( dbName, (CCP::LogType) threshold );
		if (!ret)
		{
			return PyErr_SetString(PyExc_RuntimeError, "Could not set up Sqlite logger"), 0;
		}

		Py_RETURN_TRUE;

	}

	Py_RETURN_NONE;
}

MAP_FUNCTION( "EnableSqliteLogging", PyEnableSqliteLogging, "Enables echoing of log to a sqlite database" );

PyObject* PyDisableSqliteLogging( PyObject* self, PyObject* args )
{
	if ( CCP::s_sqliteLogger != NULL )
	{
		CCP::s_sqliteLogger->Stop();
		CCP::UnregisterLogEcho( &CCP::ForwardToSqlite );
		CCP_DELETE CCP::s_sqliteLogger;
		CCP::s_sqliteLogger = NULL;
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

MAP_FUNCTION( "DisableSqliteLogging", PyDisableSqliteLogging, "Disabled sqlite logging if it has been enabled" );

#endif
