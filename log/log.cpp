#include "log.h"


void LOG::LogHint(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[32m[H] %s\n\033[0m", chBuf);
	va_end( args );
}


void LOG::LogWarn(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[33m[W] %s\n\033[0m", chBuf);
	va_end( args );
}



void LOG::LogErr(s8* pchFormat, ...)
{
	s8 chBuf[MAX_LOG_BUF_SIZE] = {0};

	va_list args;
	va_start( args, pchFormat );
	vsprintf( chBuf, pchFormat, args );
	printf("\033[31m[E] %s\n\033[0m", chBuf);
	va_end( args );
}


