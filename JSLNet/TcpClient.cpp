#include "StdAfx.h"
#include "TcpClient.h"


CTcpClient::CTcpClient(void)
{
	//与服务器连接的套接字
	m_svrSock = INVALID_SOCKET;
	ZeroMemory(&m_svrAddr, sizeof(NET_ADDR));
	//监听者实现接口
	m_lpListener = nullptr;

	//0:读事件，1:写事件，2:退出事件
	m_events[0] = WSA_INVALID_EVENT;
	m_events[1] = WSA_INVALID_EVENT;
	m_events[2] = WSA_INVALID_EVENT;
	//读写等待线程
	m_hWorkThread = NULL;

	//当前正在接收的数据包
	ZeroMemory(&m_RecvingData, sizeof(IO_CONTEXT));
	//当前正在发送的数据包
	ZeroMemory(&m_SendingData, sizeof(IO_CONTEXT));
	m_SentCompleted = WSA_INVALID_EVENT;

	//分发接收数据的独立线程
	m_hIoThread = NULL;
	//数据到达通知
	m_hIoNotify = NULL;

	WORD wVer = MAKEWORD(2, 2);
	WSADATA wsaData = {0};
	if (WSAStartup(wVer, &wsaData) != 0)
	{
		DbgLog("%s call WSAStartup error: %d", __FUNCTION__, GetLastError());
	}
}

CTcpClient::~CTcpClient(void)
{
	UnInit();

	WSACleanup();
}

DWORD CTcpClient::Release()
{
	delete this;

	return JSL_ERR_SUCCESS;
}

DWORD CTcpClient::Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener )
{
	if (lpAddr == NULL || strlen(lpAddr->szIP) < 7 || lpAddr->usPort < 1024 || lpAddr->usPort > 65535 || lpListener == NULL)
	{
		return JSL_ERR_INVALID_PARAM;
	}
	DbgView("%s prepare to connect to %s:%d", __FUNCTION__, lpAddr->szIP, lpAddr->usPort);
	
	DWORD dwRet = 0;
	do 
	{
		// 创建各种等待事件
		m_events[0] = WSACreateEvent();
		m_events[1] = WSACreateEvent();
		m_events[2] = WSACreateEvent();
		if (m_events[0] == WSA_INVALID_EVENT || m_events[1] == WSA_INVALID_EVENT || m_events[2] == WSA_INVALID_EVENT)
		{
			DbgLog("%s call WSACreateEvent error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		m_SentCompleted = WSACreateEvent();
		if (m_SentCompleted == WSA_INVALID_EVENT)
		{
			DbgLog("%s call WSACreateEvent error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		// 生成Socket并建立连接
		m_svrSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (m_svrSock == INVALID_SOCKET)
		{
			DbgLog("%s call WSASocket error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_CREATE_SOCKET;
			break;
		}
		SOCKADDR_IN remote;
		remote.sin_addr.s_addr = inet_addr(lpAddr->szIP);
		remote.sin_family      = AF_INET;
		remote.sin_port        = htons(lpAddr->usPort);
		if (connect(m_svrSock, (SOCKADDR*)&remote, sizeof(SOCKADDR)) == SOCKET_ERROR)
		{
			DbgLog("%s connect to %s:%d failed, error: %d", __FUNCTION__, lpAddr->szIP, lpAddr->usPort, WSAGetLastError());
			dwRet = JSL_ERR_NET_CONNECT;
			break;
		}
		SetKeepAlive(m_svrSock);

		m_RecvingData.sSocket = m_svrSock;
		m_RecvingData.Overlapped.hEvent = m_events[0];
		m_RecvingData.opType = OP_READ_HEAD;
		m_RecvingData.wsabuf.buf = (LPSTR)&m_RecvingData.packHead;
		m_RecvingData.wsabuf.len = sizeof(NET_PACKET_HEAD);
		m_RecvingData.dwByteRecved = 0;
		m_RecvingData.dwBytesToRecv = sizeof(NET_PACKET_HEAD);
		m_RecvingData.lpPacket = nullptr;

		m_SendingData.sSocket = m_svrSock;
		m_SendingData.Overlapped.hEvent = m_events[1];
		m_SendingData.lpPacket = nullptr;

		m_hIoNotify = CreateSemaphore(NULL, 0, 0x10000000, NULL);
		if (m_hIoNotify == NULL)
		{
			DbgLog("%s CreateSemaphore error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		m_IsStoped = FALSE;

		UINT uThreadID = 0;
		m_hWorkThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, &uThreadID);
		if (m_hWorkThread == NULL)
		{
			DbgLog("%s create worker thread error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		m_hIoThread = (HANDLE)_beginthreadex(NULL, 0, DispatchThread, this, 0, &uThreadID);
		if (m_hIoThread == NULL)
		{
			DbgLog("%s create io thread error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

	} while (FALSE);

	if (dwRet != 0)
	{
		UnInit();
	}
	else
	{
		m_svrAddr = *lpAddr;
		m_lpListener = lpListener;

		QueueUserWorkItem(FireConnectEvent, this, 0);

		DbgLog("%s successfully connected to %s:%d", __FUNCTION__, lpAddr->szIP, lpAddr->usPort);
	}
	return dwRet;
}

DWORD CTcpClient::UnInit()
{
	m_IsStoped = TRUE;

	//关闭连接
	if (m_svrSock != INVALID_SOCKET)
	{
		SafeCloseSocket(m_svrSock);

		m_svrSock = INVALID_SOCKET;
	}

	//结束等待读写线程
	if (m_events[2] != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_events[2]);
		WSACloseEvent(m_events[2]);
		m_events[2] = WSA_INVALID_EVENT;
	}
	CloseThread(&m_hWorkThread, 5);

	//结束数据通知线程
	if (m_hIoNotify != NULL)
	{
		ReleaseSemaphore(m_hIoNotify, 0x1, NULL);
		CloseHandle(m_hIoNotify);
		m_hIoNotify = NULL;
	}
	CloseThread(&m_hIoThread, 5);

	//释放读写事件
	if (m_events[0] != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_events[0]);
		WSACloseEvent(m_events[0]);
		m_events[0] = WSA_INVALID_EVENT;
	}
	if (m_events[1] != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_events[1]);
		WSACloseEvent(m_events[1]);
		m_events[1] = WSA_INVALID_EVENT;
	}
	if (m_SentCompleted != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_SentCompleted);
		WSACloseEvent(m_SentCompleted);
		m_SentCompleted = WSA_INVALID_EVENT;
	}

	//释放已经接收的数据包
	CAutoLock lockMsg(&m_RecvListLock);
	while (!m_RecvList.empty())
	{
		LPNET_PACKET lpPack = m_RecvList.front();
		FreeMemory(lpPack);

		m_RecvList.pop();
	}

	m_lpListener = nullptr;

	ZeroMemory(&m_svrAddr, sizeof(NET_ADDR));

	if (m_RecvingData.lpPacket != nullptr)
	{
		FreeMemory(m_RecvingData.lpPacket);
	}
	ZeroMemory(&m_RecvingData, sizeof(IO_CONTEXT));
	if (m_SendingData.lpPacket != nullptr)
	{
		FreeMemory(m_SendingData.lpPacket);
	}
	ZeroMemory(&m_SendingData, sizeof(IO_CONTEXT));

	return JSL_ERR_SUCCESS;
}

DWORD CTcpClient::SendData( IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync )
{
	if (dwNetID != (DWORD)m_svrSock || lpDataBuf == nullptr || dwLen == 0)
	{
		return JSL_ERR_INVALID_PARAM;
	}
	if (m_SendingData.lpPacket != nullptr)
	{
		return JSL_ERR_NET_BUSY;
	}
	DWORD dwRet = 0;
	do 
	{
		//创建数据包
		m_SendingData.opType = OP_SEND_HEAD;
		m_SendingData.packHead.dwMark = NET_PACKET_HEAD_MARK;
		m_SendingData.packHead.dwDataLen = dwLen;
		m_SendingData.wsabuf.buf = (LPSTR)&m_SendingData.packHead;
		m_SendingData.wsabuf.len = sizeof(m_SendingData.packHead);
		m_SendingData.dwByteRecved = 0;
		m_SendingData.dwBytesToRecv = sizeof(m_SendingData.packHead);
		int packLen = sizeof(NET_PACKET) + dwLen;
		m_SendingData.lpPacket = (LPNET_PACKET)AllocMemory(packLen);
		m_SendingData.lpPacket->dwProcessed = 0;
		m_SendingData.lpPacket->dwTotalLen = dwLen;
		CopyMemory(m_SendingData.lpPacket->lpBuffer, lpDataBuf, dwLen);

		//重置事件
		m_SendResult = 0;
		WSAResetEvent(m_SentCompleted);

		//发送数据
		DWORD dwFlag = 0, dwByteSent = 0;
		if (WSASend(m_SendingData.sSocket, &m_SendingData.wsabuf, 1, &dwByteSent, dwFlag, &m_SendingData.Overlapped, NULL) == SOCKET_ERROR)
		{
			DWORD dwError = WSAGetLastError();
			if (dwError != WSA_IO_PENDING)
			{
				DbgLog("% send data error: %d", __FUNCTION__, dwError);
				dwRet = JSL_ERR_SEND_NET_DATA;
				break;
			}
		}

		//等待发送完成
		if (!bAsync)
		{
			if (WSAWaitForMultipleEvents(1, &m_SentCompleted, TRUE, 180000, FALSE) != WSA_WAIT_EVENT_0)
			{
				dwRet = WSAGetLastError();
				if (dwRet != ERROR_TIMEOUT)
				{
					dwRet = JSL_ERR_SEND_NET_DATA;
				}
				else
				{
					dwRet = JSL_ERR_NET_TIMEOUT;
				}
				break;
			}

			dwRet = m_SendResult;
		}
		
	} while (FALSE);

	return dwRet;
}

DWORD CTcpClient::GetConnectList( OUT LPDWORD lpIDList, IN OUT DWORD & dwCount )
{
	if (lpIDList == nullptr)
	{
		dwCount = 1;
		return JSL_ERR_SUCCESS;
	}
	else if (dwCount < 1)
	{
		dwCount = 1;
		return JSL_ERR_INSUFFICIENT_BUFFER;
	}
	lpIDList[0] = (DWORD)m_svrSock;
	
	return JSL_ERR_SUCCESS;
}

DWORD CTcpClient::GetConnectInfo( IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr )
{
	if (dwNetID != (DWORD)m_svrSock)
	{
		return JSL_ERR_INVALID_PARAM;
	}

	GetSocketAddr(m_svrSock, lpLocalAddr, lpPeerAddr);

	return JSL_ERR_SUCCESS;
}

DWORD CTcpClient::Close( DWORD dwNetID )
{
	if (dwNetID != (DWORD)m_svrSock)
	{
		return JSL_ERR_INVALID_PARAM;
	}

	SafeCloseSocket(m_svrSock);
	
	m_svrSock = INVALID_SOCKET;

	return JSL_ERR_SUCCESS;
}

UINT CTcpClient::WorkerThread(LPVOID lpParam)
{
	CTcpClient *lpThis = (CTcpClient*)lpParam;

	//等待之前投递第一个读请求s
	PostRecv(&lpThis->m_RecvingData);

	DWORD dwRet = 0;
	while (!lpThis->m_IsStoped)
	{
		DWORD dwWait = WSAWaitForMultipleEvents(3, lpThis->m_events, FALSE, WSA_INFINITE, FALSE);
		if (WSA_WAIT_TIMEOUT == dwWait)
		{
			continue;
		}
		if (WSA_WAIT_FAILED == dwWait)  
		{
			DbgLog("%s wait error: %d", __FUNCTION__, dwWait);
			dwRet = dwWait;
			break;  
		}
		dwWait -= WSA_WAIT_EVENT_0;
		WSAResetEvent(lpThis->m_events[dwWait]);

		DWORD dwSize = 0, dwFlag = 0;
		if (dwWait == 0)	//读
		{
			if (!WSAGetOverlappedResult(lpThis->m_svrSock, (LPWSAOVERLAPPED)&lpThis->m_RecvingData, &dwSize, TRUE, &dwFlag) || dwSize == 0)
			{
				DbgLog("%s call WSAGetOverlappedResult for read error: %d, IO size: %d", __FUNCTION__, WSAGetLastError(), dwSize);
				dwRet = JSL_ERR_NET_DISCONNECTED;
				break;
			}
			else
			{
				lpThis->OnRecv(&lpThis->m_RecvingData, dwSize);
			}
		}
		else if (dwWait == 1)	//写
		{
			if (!WSAGetOverlappedResult(lpThis->m_svrSock, (LPWSAOVERLAPPED)&lpThis->m_SendingData, &dwSize, TRUE, &dwFlag) || dwSize == 0)
			{
				DbgLog("%s call WSAGetOverlappedResult for write error: %d, IO size: %d", __FUNCTION__, WSAGetLastError(), dwSize);
				dwRet = JSL_ERR_NET_DISCONNECTED;
				break;
			}
			else
			{
				lpThis->OnSend(&lpThis->m_SendingData, dwSize);
			}
		}
		else
		{
			DbgLog("%s exit, stop event is set", __FUNCTION__);
			break;
		}
	}

	if (dwRet != 0)
	{
		lpThis->OnClose(dwRet);
	}

	return 0;
}

UINT CTcpClient::DispatchThread(LPVOID lpParam)
{
	CTcpClient *lpThis = (CTcpClient*)lpParam;

	while (WaitForSingleObject(lpThis->m_hIoNotify, INFINITE) == WAIT_OBJECT_0)
	{
		if (lpThis->m_IsStoped)
		{
			DbgView("%s stop event is set, exit now", __FUNCTION__);
			break;
		}

		do
		{
			LPNET_PACKET lpPack = nullptr;
			if (!lpThis->m_RecvList.empty())
			{
				CAutoLock lock(&lpThis->m_RecvListLock);
				lpPack = lpThis->m_RecvList.front();
				lpThis->m_RecvList.pop();
			}
			if (lpPack == nullptr)
			{
				break;
			}

			if (lpThis->m_lpListener != nullptr)
			{
				lpThis->m_lpListener->OnReceive(lpThis->m_svrSock, lpPack->lpBuffer, lpPack->dwTotalLen, 0);
			}

			FreeMemory(lpPack);

		} while (!lpThis->m_IsStoped);
		
		if (lpThis->m_IsStoped)
		{
			DbgView("%s stop event is set, exit now", __FUNCTION__);
			break;
		}	
	}
	return 0;
}

DWORD CTcpClient::FireConnectEvent(LPVOID lpParam)
{
	CTcpClient *lpClient = (CTcpClient*)lpParam;

	if (lpClient->m_lpListener != NULL)
	{
		DbgView("%s before notify successfully connect to the server %s:%d", __FUNCTION__, lpClient->m_svrAddr.szIP, lpClient->m_svrAddr.usPort);
		lpClient->m_lpListener->OnConnect(lpClient->m_svrSock, &lpClient->m_svrAddr, 0);
		DbgView("%s after notify connect event", __FUNCTION__);
	}

	return ERROR_SUCCESS;
}

DWORD CTcpClient::FireCloseEvent(LPVOID lpParam)
{
	CTcpClient *lpClient = (CTcpClient*)lpParam;

	if (lpClient->m_lpListener != NULL)
	{
		DbgView("%s before notify disconnect from the server %s:%d", __FUNCTION__, lpClient->m_svrAddr.szIP, lpClient->m_svrAddr.usPort);
		lpClient->m_lpListener->OnClose(lpClient->m_svrSock, &lpClient->m_svrAddr, 0);
		DbgView("%s after notify disconnect event", __FUNCTION__);
	}

	return ERROR_SUCCESS;
}

void CTcpClient::OnRecv(LPIO_CONTEXT lpContext, DWORD dwSize)
{
	lpContext->dwByteRecved += dwSize;
	if (lpContext->dwByteRecved < lpContext->dwBytesToRecv)		//此次需要接收的数据没有接收完成，继续接收
	{
		lpContext->wsabuf.buf += dwSize;
		lpContext->wsabuf.len -= dwSize;
	}
	else if (lpContext->opType == OP_READ_HEAD)		//接收到一个数据包的头部
	{
		if (lpContext->packHead.dwDataLen == 0)	//空数据包
		{
			lpContext->wsabuf.len = sizeof(NET_PACKET_HEAD);
			lpContext->wsabuf.buf = (LPSTR)&lpContext->packHead;
			lpContext->dwByteRecved = 0;
			lpContext->dwBytesToRecv = sizeof(NET_PACKET_HEAD);
		}
		else	//分配相应空间接收剩余的数据体
		{
			lpContext->opType = OP_READ_DATA;

			DWORD dwPackLen = sizeof(NET_PACKET) + lpContext->packHead.dwDataLen;
			lpContext->lpPacket = (LPNET_PACKET)AllocMemory(dwPackLen);
			lpContext->lpPacket->dwProcessed = 0;
			lpContext->lpPacket->dwTotalLen = lpContext->packHead.dwDataLen;
			ZeroMemory(lpContext->lpPacket->lpBuffer, lpContext->lpPacket->dwTotalLen);
			
			DWORD dwRemain = lpContext->lpPacket->dwTotalLen - lpContext->lpPacket->dwProcessed;
			if (dwRemain > MAX_PACKET_LEN)
			{
				dwRemain = MAX_PACKET_LEN;
			}
			lpContext->wsabuf.len = dwRemain;
			lpContext->wsabuf.buf = LPSTR(lpContext->lpPacket->lpBuffer + lpContext->lpPacket->dwProcessed);
			lpContext->dwByteRecved = 0;
			lpContext->dwBytesToRecv = dwRemain;
		}
	}
	else	//接收到数据
	{
		lpContext->lpPacket->dwProcessed += lpContext->dwByteRecved;

		if (lpContext->lpPacket->dwProcessed < lpContext->lpPacket->dwTotalLen)	//继续接收下一个数据分组
		{
			DWORD dwRemain = lpContext->lpPacket->dwTotalLen - lpContext->lpPacket->dwProcessed;
			if (dwRemain > MAX_PACKET_LEN)
			{
				dwRemain = MAX_PACKET_LEN;
			}
			lpContext->wsabuf.len = dwRemain;
			lpContext->wsabuf.buf = LPSTR(lpContext->lpPacket->lpBuffer + lpContext->lpPacket->dwProcessed);
			lpContext->dwByteRecved = 0;
			lpContext->dwBytesToRecv = dwRemain;
		}
		else
		{
			//数据接收完毕，加入队列
			CAutoLock lock(&m_RecvListLock);
			m_RecvList.push(lpContext->lpPacket);
			ReleaseSemaphore(m_hIoNotify, 1, NULL);

			//重新接收头部
			lpContext->wsabuf.len = sizeof(NET_PACKET_HEAD);
			lpContext->wsabuf.buf = (LPSTR)&lpContext->packHead;
			lpContext->opType = OP_READ_HEAD;
			lpContext->dwByteRecved = 0;
			lpContext->dwBytesToRecv = sizeof(NET_PACKET_HEAD);
			lpContext->lpPacket = nullptr;
		}
	}

	PostRecv(lpContext);
}

void CTcpClient::OnSend(LPIO_CONTEXT lpContext, DWORD dwSize)
{
	DWORD dwSendResult = 0;
	do 
	{
		lpContext->dwByteRecved += dwSize;
		if (lpContext->dwByteRecved < lpContext->dwBytesToRecv)
		{
			//当次分包没有发送完，继续发送
			lpContext->wsabuf.buf += dwSize;
			lpContext->wsabuf.len -= dwSize;
			if (!PostSend(lpContext))
			{
				dwSendResult = JSL_ERR_SEND_NET_DATA;
			}
			break;
		}
		if (lpContext->opType == OP_SEND_HEAD)	//头部发送完成，发送第一个数据分包
		{
			lpContext->opType = OP_SEND_DATA;
		}
		else
		{
			lpContext->lpPacket->dwProcessed += lpContext->dwByteRecved;
		}

		if (lpContext->lpPacket->dwProcessed == lpContext->lpPacket->dwTotalLen)
		{
			//发送完成
			FreeMemory(lpContext->lpPacket);
			lpContext->lpPacket = nullptr;

			m_SendResult = 0;
			WSASetEvent(m_SentCompleted);

			if (m_lpListener != nullptr)
			{
				m_lpListener->OnSent(m_svrSock, m_SendResult);
			}
		}
		else
		{
			//继续发送数据
			DWORD dwBytesRemain = lpContext->lpPacket->dwTotalLen - lpContext->lpPacket->dwProcessed;
			if (dwBytesRemain > MAX_PACKET_LEN)
			{
				lpContext->dwBytesToRecv = MAX_PACKET_LEN;
			}
			else
			{
				lpContext->dwBytesToRecv = dwBytesRemain;
			}
			lpContext->dwByteRecved = 0;
			lpContext->wsabuf.buf = LPSTR(lpContext->lpPacket->lpBuffer + lpContext->lpPacket->dwProcessed);
			lpContext->wsabuf.len = lpContext->dwBytesToRecv;
			if (!PostSend(lpContext))
			{
				dwSendResult = JSL_ERR_SEND_NET_DATA;
				break;
			}
		}
	} while (FALSE);

	if (dwSendResult != 0)
	{
		FreeMemory(lpContext->lpPacket);
		lpContext->lpPacket = nullptr;

		m_SendResult = dwSendResult;
		WSASetEvent(m_SentCompleted);

		if (m_lpListener != nullptr)
		{
			m_lpListener->OnSent(m_svrSock, m_SendResult);
		}
	}
}

void CTcpClient::OnClose(DWORD dwErr)
{
	SafeCloseSocket(m_svrSock);
	m_svrSock = INVALID_SOCKET;

	QueueUserWorkItem(FireCloseEvent, this, 0);
}