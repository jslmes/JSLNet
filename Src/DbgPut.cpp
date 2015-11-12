#include "stdafx.h"

void WriteLog(LPCSTR lpLogStr)
{
	if (GetFileSize(g_hLogFile, NULL) > _MAX_LOG_SIZE)
	{
		SetFilePointer(g_hLogFile, 0, NULL, FILE_BEGIN);
		SetEndOfFile(g_hLogFile);
	}
	SetFilePointer(g_hLogFile, 0, NULL, FILE_END);

	SYSTEMTIME curTime;
	GetLocalTime(&curTime);
	char strTemp[LOG_LEN] = {0};
	_snprintf_s(strTemp, LOG_LEN, LOG_LEN - 1, "[%.4d-%.2d-%.2d %.2d:%.2d:%.2d]  %s\r\n", curTime.wYear, curTime.wMonth, curTime.wDay,
		curTime.wHour, curTime.wMinute, curTime.wSecond, lpLogStr);
	DWORD dwBytes = 0;
	WriteFile(g_hLogFile, strTemp, strlen(strTemp) * sizeof(char), &dwBytes, NULL);
}

void Log(LPCSTR szFormat, ...)
{
	CHAR szLog[LOG_LEN] = _MODULE_NAME;

	va_list args;
	va_start(args, szFormat);
	_vsnprintf_s(&szLog[strlen(szLog)], LOG_LEN - strlen(szLog), LOG_LEN - strlen(szLog) - 1, szFormat, args);
	va_end(args);

	OutputDebugStringA(szLog);
	if (g_hLogFile != INVALID_HANDLE_VALUE)
	{
		WriteLog(szLog);
	}
}

void Debug(LPCSTR szFormat, ...)
{
	CHAR szLog[LOG_LEN] = _MODULE_NAME;

	va_list args;
	va_start(args, szFormat);
	_vsnprintf_s(&szLog[strlen(szLog)], LOG_LEN - strlen(szLog), LOG_LEN - strlen(szLog) - 1, szFormat, args);
	va_end(args);

	OutputDebugStringA(szLog);
}

void Log(LPCWSTR wszFormat, ...)
{
	WCHAR wszLog[LOG_LEN] = {0};
	wsprintfW(wszLog, L"%S", _MODULE_NAME);

	va_list args;
	va_start(args, wszFormat);
	_vsnwprintf_s(&wszLog[wcslen(wszLog)], LOG_LEN - wcslen(wszLog), LOG_LEN - wcslen(wszLog) - 1, wszFormat, args);
	va_end(args);

	OutputDebugStringW(wszLog);
	if (g_hLogFile != INVALID_HANDLE_VALUE)
	{
		char szLog[LOG_LEN] = {0};
		WideCharToMultiByte(CP_ACP, 0, wszLog, -1, szLog, LOG_LEN - 1, NULL, NULL);
		WriteLog(szLog);
	}
}

void Debug(LPCWSTR wszFormat, ...)
{
	WCHAR wszLog[LOG_LEN] = {0};
	wsprintfW(wszLog, L"%S", _MODULE_NAME);

	va_list args;
	va_start(args, wszFormat);
	_vsnwprintf_s(&wszLog[wcslen(wszLog)], LOG_LEN - wcslen(wszLog), LOG_LEN - wcslen(wszLog) - 1, wszFormat, args);
	va_end(args);

	OutputDebugStringW(wszLog);
}