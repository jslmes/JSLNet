#include "StdAfx.h"
#include "Utils.h"


void SetKeepAlive(SOCKET sock)
{
	BOOL bKeepAlive = TRUE;
	::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(bKeepAlive));
	// 设置KeepAlive参数
	tcp_keepalive alive_in        = {0};
	tcp_keepalive alive_out       = {0};
	alive_in.keepalivetime        = 30000;                // 开始首次KeepAlive探测前的TCP空闭时间
	alive_in.keepaliveinterval    = 5000;                // 两次KeepAlive探测间的时间间隔
	alive_in.onoff                = TRUE;
	unsigned long ulBytesReturn = 0;
	WSAIoctl(sock, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in), &alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL);
}

void CloseThread(HANDLE * lpThread, DWORD dwSeconds)
{
	if (*lpThread == NULL)
	{
		return;
	}
	if (WaitForSingleObject(*lpThread, dwSeconds * 1000) != WAIT_OBJECT_0)
	{
		TerminateThread(*lpThread, 0x1);
	}
	CloseHandle(*lpThread);
	*lpThread = NULL;
}

BOOL GetSocketAddr(SOCKET sock, LPNET_ADDR lpLocal, LPNET_ADDR lpRemote)
{
	SOCKADDR_IN local = {0}, remote = {0};
	int iLen = sizeof(SOCKADDR_IN);

	if (lpLocal != NULL)
	{
		if (getsockname(sock, (SOCKADDR*)&local, &iLen) == SOCKET_ERROR)
		{
			DbgLog("%s call getsockname error: %d", __FUNCTION__, WSAGetLastError());
			return FALSE;
		}
		strncpy_s(lpLocal->szIP, NET_IP_LEN, inet_ntoa(local.sin_addr), NET_IP_LEN - 1);
		lpLocal->usPort = ntohs(local.sin_port);
	}

	if (lpRemote != NULL)
	{
		if (getpeername(sock, (SOCKADDR*)&remote, &iLen) == SOCKET_ERROR)
		{
			DbgLog("%s call getpeername error: %d", __FUNCTION__, WSAGetLastError());
			return FALSE;
		}
		strncpy_s(lpRemote->szIP, NET_IP_LEN, inet_ntoa(remote.sin_addr), NET_IP_LEN - 1);
		lpRemote->usPort = ntohs(remote.sin_port);
	}

	return ERROR_SUCCESS;
}

void SafeCloseSocket(SOCKET & sock)
{
	if (sock != INVALID_SOCKET)
	{
		LINGER  lingerStruct;
		lingerStruct.l_onoff = 1;
		lingerStruct.l_linger = 30;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));
		closesocket(sock);
		sock = INVALID_SOCKET;
	}
}

BOOL PostRecv(LPIO_CONTEXT pContext)
{
	DWORD dwFlag = 0, dwBytesRecved = 0;
	if (WSARecv(pContext->sSocket, &pContext->wsabuf, 1, &dwBytesRecved, &dwFlag, &pContext->Overlapped, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DbgLog("%s call WSARecv error: %d", __FUNCTION__, WSAGetLastError());
			return FALSE;
		}
	}
	return TRUE;
}

BOOL PostSend(LPIO_CONTEXT pContext)
{
	DWORD dwFlag = 0, dwByteSent = 0;
	if (WSASend(pContext->sSocket, &pContext->wsabuf, 1, &dwByteSent, dwFlag, &pContext->Overlapped, NULL) == SOCKET_ERROR)  //发送失败，删除动态数据
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DbgLog("%s call WSASend error: %d", __FUNCTION__, WSAGetLastError());
			return FALSE;
		}
	}
	return TRUE;
}