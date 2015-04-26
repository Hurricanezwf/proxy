#ifndef LOG_H_
#define LOG_H_

#include "base.h"
#include <stdarg.h>

namespace LOG
{

#define		MAX_LOG_BUF_SIZE		512



//normal print
void LogHint(s8* pchFormat, ...);

//warning print
void LogWarn(s8* pchFormat, ...);

//error print
void LogErr(s8* pchFormat, ...);



	
}



#endif  //LOG_H_
