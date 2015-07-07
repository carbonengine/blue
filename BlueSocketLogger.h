#pragma once
#ifndef BlueSocketLogger_H
#define BlueSocketLogger_H

bool StartSocketLogger( const char* ip = "127.0.0.1", int port = 0xCC9 );
void StopSocketLogger();

void LogToSocketLogger( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message );

#endif