// Test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <Windows.h>
#include <process.h>

#include "../JSLNet/JSLNet.h"
#pragma comment(lib, "../Lib/Debug/JSLNet.lib")

class CServer : public IJslNetListener
{
	virtual DWORD OnAccept( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		printf("Accept a conection(%d) from: %s\n", dwNetID, lpPeerAddr->szIP);

		return 0;
	}

	virtual DWORD OnConnect( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		printf("Server inited.\n");

		return 0;
	}

	virtual DWORD OnClose( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		printf("Connection(%d) closed.\n", dwNetID);

		return 0;
	}

	virtual DWORD OnReceive( IN DWORD dwPeerID, IN LPBYTE lpBuffer, IN DWORD dwBufferLen, IN DWORD dwErr ) 
	{
		printf("Receive %d bytes from connection(%d): %s.\n", dwBufferLen, dwPeerID, (LPSTR)lpBuffer);

		return 0;
	}

	virtual DWORD OnSent( IN DWORD dwNetID, IN DWORD dwErr ) 
	{
		printf("Sent successfully to connection(%d).\n", dwNetID);

		return 0;
	}
};

#include "../Inc/Lock.h"

int _tmain(int argc, _TCHAR* argv[])
{
	CServer svr;

	NET_ADDR addr = {0};
	addr.usPort = 7033;

	IJslTcp *lpTcp = nullptr;
	CreateJslNetPtr(0, TRUE, &lpTcp);

	DWORD dwRet = lpTcp->Init(&addr, &svr);
	if (dwRet != 0)
	{
		printf("%s init error: 0x%x\n", __FUNCTION__, dwRet);
	}
	else
	{
		printf("Net inited, enter any char to exit\n");
	}

	getchar();

	lpTcp->UnInit();
	lpTcp->Release();

	getchar();

	return 0;
}

