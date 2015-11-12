#ifndef _EASY_BACKUP_DEBUG_PUT_JAKE_JIANGE_
#define _EASY_BACKUP_DEBUG_PUT_JAKE_JIANGE_

#include <stdio.h>

#ifndef _MODULE_NAME
#define _MODULE_NAME	"[Other] "
#endif

#ifndef _MAX_LOG_SIZE
#define _MAX_LOG_SIZE	(2 * 1024 * 1024)
#endif

#ifndef LOG_LEN
#define LOG_LEN 1024
#endif

// 日志文件句柄，需要在程序文件中定义
extern HANDLE g_hLogFile;

void WriteLog(LPCSTR lpLogStr);

void Log(LPCSTR szFormat, ...);

void Debug(LPCSTR szFormat, ...);

void Log(LPCWSTR wszFormat, ...);

void Debug(LPCWSTR wszFormat, ...);

#define DbgLog	Log
#define DbgView	Log


#endif