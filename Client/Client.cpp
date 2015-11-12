// Client.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <process.h>
#include "../JSLNet/JSLNet.h"
#pragma comment(lib, "../Lib/Debug/JSLNet.lib")

class CClient : public IJslNetListener
{
private:
	DWORD m_dwNetId;
	BOOL m_bStoped;
	HANDLE m_hThread;
	IJslTcp *m_lpTcp;

public:

	DWORD Start()
	{
		m_bStoped = FALSE;
		m_dwNetId = 0;
		m_hThread = NULL;
		m_lpTcp = NULL;

		NET_ADDR addr = {0};
		strcpy_s(addr.szIP, NET_IP_LEN, "127.0.0.1");
		addr.usPort = 7033;

		CreateJslNetPtr(0, FALSE, &m_lpTcp);

		DWORD dwRet = m_lpTcp->Init(&addr, this);
		if (dwRet != 0)
		{
			printf("%s init error: 0x%x\n", __FUNCTION__, dwRet);
		}
		else
		{
			while (m_dwNetId == 0)
			{
				Sleep(100);
			}

			UINT uID = 0;
			m_hThread = (HANDLE)_beginthreadex(nullptr, 0, WorkThread, this, 0, &uID);
		}

		return 0;
	}

	DWORD Stop()
	{
		m_bStoped = FALSE;

		if (WaitForSingleObject(m_hThread, 2000) != WAIT_OBJECT_0)
		{
			TerminateThread(m_hThread, 1);
		}

		CloseHandle(m_hThread);
		m_hThread = NULL;

		m_lpTcp->UnInit();
		m_lpTcp->Release();
		m_lpTcp = NULL;

		return 0;
	}

private:

	static UINT WINAPI WorkThread(LPVOID lpParam)
	{
		CClient *lpThis = (CClient*)lpParam;

		char szBuf[100] = "1234567890123456789012345678901234567890";
		int iLen = strlen(szBuf) + 1;

	//	while (!lpThis->m_bStoped)
		for (int i = 0; i < 100; i++)
		{
			DWORD dwRet = lpThis->m_lpTcp->SendData(lpThis->m_dwNetId, (LPBYTE)szBuf, strlen(szBuf) + 1, FALSE);
			if (dwRet != 0)
			{
				printf("%s send error: 0x%x\n", __FUNCTION__, dwRet);
				break;
			}
		}

		return 0;
	}

	virtual DWORD OnAccept( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		printf("Accept a conection(%d) from: %s\n", dwNetID, lpPeerAddr->szIP);

		return 0;
	}

	virtual DWORD OnConnect( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		m_dwNetId = dwNetID;

		printf("Connected successfully.\n");

		return 0;
	}

	virtual DWORD OnClose( IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr ) 
	{
		printf("Connection(%d) closed.\n", m_dwNetId);

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

int _tmain(int argc, _TCHAR* argv[])
{
	CClient clt1;
	CClient clt2;
	CClient clt3;

	clt1.Start();
	clt2.Start();
	clt3.Start();

	getchar();

	clt1.Stop();
	clt2.Stop();
	clt3.Stop();

	getchar();
	return 0;
}

