// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"

extern HANDLE	g_hHeap = NULL;
extern HANDLE	g_hLogFile = INVALID_HANDLE_VALUE;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			g_hHeap = HeapCreate(0, 0x2000, 0);

			char szPath[MAX_PATH] = {0};
			GetModuleFileNameA(NULL, szPath, MAX_PATH - 1);
			*(strrchr(szPath, '\\') + 1) = '\0';
			strcat_s(szPath, MAX_PATH, "Net.log");
			g_hLogFile = CreateFileA(szPath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		{
			if (g_hHeap!= NULL)
			{
				HeapDestroy(g_hHeap);
				g_hHeap = NULL;
			}
			if (g_hLogFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(g_hLogFile);
				g_hLogFile = INVALID_HANDLE_VALUE;
			}
		}
		break;
	}
	return TRUE;
}

