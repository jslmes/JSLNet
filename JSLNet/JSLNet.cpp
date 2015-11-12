// JSLNet.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "JSLNet.h"
#include "TcpClient.h"
#include "TcpServer.h"


DWORD WINAPI CreateJslNetPtr(DWORD dwFlag, BOOL IsServer, IJslTcp ** lppJslNet)
{
	if (lppJslNet == nullptr)
	{
		return JSL_ERR_INVALID_PARAM;
	}

	if (IsServer)
	{
		*lppJslNet = new CTcpServer();
	}
	else
	{
		*lppJslNet = new CTcpClient();
	}

	if ((*lppJslNet) == nullptr)
	{
		return JSL_ERR_OUT_OF_MEMEORY;
	}

	return JSL_ERR_SUCCESS;
}

DWORD WINAPI GetJslNetError(DWORD dwErrorCode, LPWSTR lpszErrMsg, int iErrLen)
{
	DWORD dwCode = dwErrorCode & 0xFFF;

	return JSL_ERR_SUCCESS;
}