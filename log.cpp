#include "log.h"


void LOG::LogHint(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[32m%s\n\033[0m", chBuf);
	va_end( args );
}


void LOG::LogWarn(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[33m%s\n\033[0m", chBuf);
	va_end( args );
}



void LOG::LogErr(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[31m%s\n\033[0m", chBuf);
	va_end( args );
}


