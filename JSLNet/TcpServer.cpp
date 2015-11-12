#include "StdAfx.h"
#include "TcpServer.h"


CTcpServer::CTcpServer(void)
{
	m_lpListener = nullptr;

	m_IsStoped = TRUE;
	m_svrSock = INVALID_SOCKET;
	m_hAcceptThread = NULL;
	m_acceptEvents[0] = WSA_INVALID_EVENT;
	m_acceptEvents[1] = WSA_INVALID_EVENT;
	m_hLinkThread = NULL;
	m_waitLinks.clear();
	m_hLinkNotify = NULL;

	m_hIocp = NULL;
	m_threadPool.clear();

	m_IoLinkList.clear();

	m_hIoThread = NULL;
	m_hIoNotify = NULL;

	//当前正在发送的数据包
	ZeroMemory(&m_SendingData, sizeof(IO_CONTEXT));
	//发送完成通知事件
	m_SentCompleted = WSA_INVALID_EVENT;
	//发送结果
	m_SendResult = 0;

	WORD wVer = MAKEWORD(2, 2);
	WSADATA wsaData = {0};
	if (WSAStartup(wVer, &wsaData) != 0)
	{
		DbgLog("CTCPClient call WSAStartup failed, error: %d", GetLastError());
	}
}

CTcpServer::~CTcpServer(void)
{
	UnInit();

	WSACleanup();
}

DWORD CTcpServer::Release()
{
	delete this;
	return JSL_ERR_SUCCESS;
}

DWORD CTcpServer::Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener )
{
	if (lpAddr == nullptr || lpListener == nullptr || lpAddr->usPort < 1024 || lpAddr->usPort > 65535)
	{
		return JSL_ERR_INVALID_PARAM;
	}

	DWORD dwRet = JSL_ERR_SUCCESS;
	do 
	{
		m_IsStoped = FALSE;
		m_lpListener = lpListener;

		//创建等待事件
		m_acceptEvents[0] = WSACreateEvent();
		if (m_acceptEvents[0] == WSA_INVALID_EVENT)
		{
			DbgLog("%s WSACreateEvent error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		m_acceptEvents[1] = WSACreateEvent();
		if (m_acceptEvents[1] == WSA_INVALID_EVENT)
		{
			DbgLog("%s WSACreateEvent error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		//发送事件
		m_SendResult = 0;
		m_SendingData.lpPacket = nullptr;
		m_SentCompleted = WSACreateEvent();
		if (m_SentCompleted == WSA_INVALID_EVENT)
		{
			DbgLog("%s call WSACreateEvent error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		//创建完成端口
		m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		if (m_hIocp == NULL)
		{
			DbgLog("%s CreateIoCompletionPort error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		//取得主机的CPU数量，并根据数量的多少，成正比例关系的创建线程池
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		int iThreadCount = sysInfo.dwNumberOfProcessors * 2 + 2;  //最佳的线程数量

		m_threadPool.clear();
		for (int i = 0; i < iThreadCount; i++)
		{
			UINT dwThreadID = 0;
			HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, &dwThreadID);
			if (hThread == NULL)
			{
				DbgLog("%s create iocp thread error: %d", __FUNCTION__, GetLastError());
			}
			else
			{
				m_threadPool.push_back(hThread);
			}
		}
		if (m_threadPool.empty())
		{
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		//创建并设置SOCKET
		m_svrSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (m_svrSock == INVALID_SOCKET)
		{
			DbgLog("%s WSASocket error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_CREATE_SOCKET;
			break;
		}
		SOCKADDR_IN addr;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_family      = AF_INET;
		addr.sin_port        = htons(lpAddr->usPort);
		if (SOCKET_ERROR == bind(m_svrSock, (SOCKADDR*)&addr, sizeof(SOCKADDR)))
		{
			DbgLog("%s bind socket to port %d error: %d", __FUNCTION__, lpAddr->usPort, WSAGetLastError());
			dwRet = JSL_ERR_CREATE_SOCKET;
			break;
		}
		if (SOCKET_ERROR == listen(m_svrSock, 3))
		{
			DbgLog("%s listen socket error: %d", __FUNCTION__, WSAGetLastError());
			dwRet = JSL_ERR_CREATE_SOCKET;
			break;
		}
		if (WSAEventSelect(m_svrSock, m_acceptEvents[0], FD_ACCEPT) == SOCKET_ERROR)
		{
			dwRet = WSAGetLastError();
			DbgLog("%s call WSAEventSelect to select the accept event failed, error: %d", __FUNCTION__, dwRet);
			break;
		}

		//创建数据到达通知信号量
		m_hIoNotify = CreateSemaphore(NULL, 0, 0x10000000, NULL);
		if (m_hIoNotify == NULL)
		{
			DbgLog("%s CreateSemaphore error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		//创建连接到达通知信号量
		m_hLinkNotify = CreateSemaphore(NULL, 0, 1000, NULL);
		if (m_hLinkNotify == NULL)
		{
			DbgLog("%s CreateSemaphore error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}

		//创建连接等待线程
		UINT uThreadID = 0;
		m_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, &uThreadID);
		if (m_hAcceptThread == NULL)
		{
			DbgLog("%s create accept thread error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		m_hIoThread = (HANDLE)_beginthreadex(NULL, 0, DispatchDataThread, this, 0, &uThreadID);
		if (m_hIoThread == NULL)
		{
			DbgLog("%s create io thread error: %d", __FUNCTION__, GetLastError());
			dwRet = JSL_ERR_OUT_OF_MEMEORY;
			break;
		}
		m_hLinkThread = (HANDLE)_beginthreadex(NULL, 0, DispatchLinkThread, this, 0, &uThreadID);
		if (m_hLinkThread == NULL)
		{
			DbgLog("%s create link thread error: %d", __FUNCTION__, GetLastError());
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
		QueueUserWorkItem(FireInitedEvent, this, 0);
	}
	return dwRet;
}

DWORD CTcpServer::UnInit()
{
	m_IsStoped = TRUE;

	//停止等待连接线程
	if (m_acceptEvents[1] != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_acceptEvents[1]);
		WSACloseEvent(m_acceptEvents[1]);
		m_acceptEvents[1] = WSA_INVALID_EVENT;
	}
	if (m_hAcceptThread != NULL)
	{
		CloseThread(&m_hAcceptThread, 5);
	}
	if (m_acceptEvents[0] != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_acceptEvents[0]);
		WSACloseEvent(m_acceptEvents[0]);
		m_acceptEvents[0] = WSA_INVALID_EVENT;
	}

	//停止连接通知线程
	if (m_hLinkNotify != NULL)
	{
		ReleaseSemaphore(m_hLinkNotify, 1, NULL);

		CloseHandle(m_hLinkNotify);
		m_hLinkNotify = NULL;
	}
	if (m_hLinkThread != NULL)
	{
		CloseThread(&m_hLinkThread, 5);
	}
	CAutoLock lock(&m_waitLinkLock);
	for (auto item = m_waitLinks.begin(); item != m_waitLinks.end(); item++)
	{
		delete (LPLINK_EVENT)(*item);
	}
	m_waitLinks.clear();

	//停止数据通知线程
	if (m_hIoNotify != NULL)
	{
		ReleaseSemaphore(m_hIoNotify, 1, NULL);

		CloseHandle(m_hIoNotify);
		m_hIoNotify = NULL;
	}
	if (m_hIoThread != NULL)
	{
		CloseThread(&m_hIoThread, 5);
	}
	CAutoLock lock2(&m_RecvListLock);
	while (!m_RecvList.empty())
	{
		LPNET_PACKET lpPack = m_RecvList.front();
		FreeMemory(lpPack);

		m_RecvList.pop();
	}

	//关闭线程池
	if (m_hIocp != NULL)
	{
		CloseHandle(m_hIocp);
		m_hIocp = NULL;
	}
	for (auto item = m_threadPool.begin(); item != m_threadPool.end(); item++)
	{
		HANDLE hThread = *item;
		CloseThread(&hThread, 3);
	}
	m_threadPool.clear();

	//释放已经接收的数据
	CAutoLock lock3(&m_IoLinkLock);
	for (auto item = m_IoLinkList.begin(); item != m_IoLinkList.end(); item++)
	{
		SOCKET sock = item->first;
		SafeCloseSocket(sock);

		LPIO_CONTEXT lpContext = item->second;
		if (lpContext->lpPacket != nullptr)
		{
			FreeMemory(lpContext->lpPacket);
		}
		FreeMemory(lpContext);
	}
	m_IoLinkList.clear();

	//关闭服务器SOCKET
	if (m_svrSock != INVALID_SOCKET)
	{
		closesocket(m_svrSock);
		m_svrSock = INVALID_SOCKET;
	}

	m_lpListener = nullptr;

	//取消正在发送
	m_SendResult = 0;
	if (m_SentCompleted != WSA_INVALID_EVENT)
	{
		WSASetEvent(m_SentCompleted);
		WSACloseEvent(m_SentCompleted);
		m_SentCompleted = WSA_INVALID_EVENT;
	}
	if (m_SendingData.lpPacket != nullptr)
	{
		FreeMemory(m_SendingData.lpPacket);
	}
	ZeroMemory(&m_SendingData, sizeof(IO_CONTEXT));
	
	return JSL_ERR_SUCCESS;
}

DWORD CTcpServer::SendData( IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync )
{
	if (m_IoLinkList.find(SOCKET(dwNetID)) == m_IoLinkList.end())
	{
		return JSL_ERR_INVALID_PARAM;
	}
	if (lpDataBuf == nullptr || dwLen == 0)
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
		m_SendingData.sSocket = (SOCKET)dwNetID;
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
		if (WSASend(SOCKET(dwNetID), &m_SendingData.wsabuf, 1, &dwByteSent, dwFlag, &m_SendingData.Overlapped, NULL) == SOCKET_ERROR)
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

DWORD CTcpServer::GetConnectList( OUT LPDWORD lpIDList, IN OUT DWORD & dwCount )
{
	DWORD dwSize = m_IoLinkList.size();
	if (lpIDList == NULL)
	{
		dwCount = dwSize;
		return JSL_ERR_SUCCESS;
	}
	else if (dwCount < dwSize)
	{
		dwCount = dwSize;
		return JSL_ERR_INSUFFICIENT_BUFFER;
	}

	CAutoLock lock(&m_IoLinkLock);
	DWORD dwNum = 0;
	for (auto it = m_IoLinkList.begin(); it != m_IoLinkList.end(); it++)
	{
		lpIDList[dwNum] = it->first;
		dwNum++;
	}
	dwCount = dwNum;
	
	return JSL_ERR_SUCCESS;
}

DWORD CTcpServer::GetConnectInfo( IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr )
{
	if (m_IoLinkList.find(SOCKET(dwNetID)) == m_IoLinkList.end())
	{
		return JSL_ERR_INVALID_PARAM;
	}

	GetSocketAddr(SOCKET(dwNetID), lpLocalAddr, lpPeerAddr);

	return JSL_ERR_SUCCESS;
}

DWORD CTcpServer::Close( DWORD dwNetID )
{
	if (m_IoLinkList.find(SOCKET(dwNetID)) == m_IoLinkList.end())
	{
		return JSL_ERR_INVALID_PARAM;
	}

	SOCKET sock = (SOCKET)dwNetID;
	SafeCloseSocket(sock);

	return JSL_ERR_SUCCESS;
}

DWORD CTcpServer::FireInitedEvent(LPVOID lpThreadParameter)
{
	CTcpServer *lpThis = (CTcpServer*)lpThreadParameter;

	if (lpThis->m_lpListener != nullptr)
	{
		lpThis->m_lpListener->OnConnect(lpThis->m_svrSock, nullptr, 0);
	}

	return 0;
}

UINT WINAPI CTcpServer::AcceptThread(LPVOID lpParam)
{
	CTcpServer *lpThis = (CTcpServer*)lpParam;

	while (!lpThis->m_IsStoped)
	{
		DWORD dwWait = WSAWaitForMultipleEvents(2, lpThis->m_acceptEvents, FALSE, INFINITE, FALSE);
		if (dwWait == WSA_WAIT_TIMEOUT)
		{
			continue;
		}
		if (lpThis->m_IsStoped || dwWait == WSA_WAIT_FAILED || (dwWait - WSA_WAIT_EVENT_0) != 0)
		{
			DbgView("%s exited, stop event is set, last error: %d", __FUNCTION__, WSAGetLastError());
			break;
		}

		WSANETWORKEVENTS wsaNetworkEvent;
		if (SOCKET_ERROR == WSAEnumNetworkEvents(lpThis->m_svrSock, lpThis->m_acceptEvents[0], &wsaNetworkEvent))
		{
			DbgLog("%s call WSAEnumNetworkEvents error: %d", __FUNCTION__, WSAGetLastError());
			continue;
		}
		if (wsaNetworkEvent.lNetworkEvents & FD_ACCEPT)  //接收到一个连接
		{
			if (0 != wsaNetworkEvent.iErrorCode[FD_ACCEPT_BIT])
			{
				DbgLog("%s come a connect request, but error occur: %d", __FUNCTION__, wsaNetworkEvent.iErrorCode[FD_ACCEPT_BIT]);
				continue;
			}
			SOCKADDR_IN acceptAddr = {0};
			int iAddrLen = sizeof(SOCKADDR_IN);
			SOCKET acceptSocket = accept(lpThis->m_svrSock, (SOCKADDR*)&acceptAddr, &iAddrLen);
			if (INVALID_SOCKET == acceptSocket)
			{
				DbgLog("%s accept a invalid connect, error: %d", __FUNCTION__, WSAGetLastError());
				continue;
			}

			LPLINK_EVENT lpLink = new LINK_EVENT;
			lpLink->IsConnect = TRUE;
			lpLink->sSock = acceptSocket;
			lpLink->PeerAddr.usPort = acceptAddr.sin_port;
			strncpy_s(lpLink->PeerAddr.szIP, NET_IP_LEN, inet_ntoa(acceptAddr.sin_addr), NET_IP_LEN - 1);
			DbgView("%s accept a connect from %s, ID: %d", __FUNCTION__, lpLink->PeerAddr.szIP, acceptSocket);

			CAutoLock lock(&lpThis->m_waitLinkLock);
			lpThis->m_waitLinks.push_back(lpLink);
			ReleaseSemaphore(lpThis->m_hLinkNotify, 1, nullptr);
		}
	}
	return 0;
}

UINT WINAPI CTcpServer::DispatchLinkThread(LPVOID lpParam)
{
	CTcpServer *lpThis = (CTcpServer*)lpParam;
	while (WaitForSingleObject(lpThis->m_hLinkNotify, INFINITE) == WAIT_OBJECT_0)
	{
		if (lpThis->m_IsStoped)
		{
			DbgView("%s stop event is set, exit now", __FUNCTION__);
			break;
		}

		while (!lpThis->m_IsStoped)
		{
			LPLINK_EVENT lpLink = nullptr;
			if (!lpThis->m_waitLinks.empty())
			{
				CAutoLock lock(&lpThis->m_waitLinkLock);
				auto item = lpThis->m_waitLinks.begin();
				lpLink = *item;

				lpThis->m_waitLinks.erase(item);
			}
			if (lpLink == nullptr)
			{
				break;
			}

			//断开连接事件，仅做通知操作
			if (!lpLink->IsConnect)
			{
				if (lpThis->m_lpListener != nullptr)
				{
					lpThis->m_lpListener->OnClose(lpLink->sSock, nullptr, 0);
				}
				delete lpLink;
				continue;
			}

			BOOL InvalidSocket = TRUE;
			LPIO_CONTEXT lpContext = nullptr;
			do 
			{
				if (lpThis->m_lpListener != nullptr)
				{
					if (lpThis->m_lpListener->OnAccept(lpLink->sSock, &lpLink->PeerAddr, 0) != 0)
					{
						DbgLog("%s denie the connect from %s:%d", __FUNCTION__, lpLink->PeerAddr.szIP, lpLink->PeerAddr.usPort);
						break;
					}
				}

				//设置心跳检测
				SetKeepAlive(lpLink->sSock);

				//准备读写
				lpContext = (LPIO_CONTEXT)AllocMemory(sizeof(IO_CONTEXT));
				if (lpContext == nullptr)
				{
					DbgLog("%s alloct memory error: %d", __FUNCTION__, GetLastError());
					break;
				}
				lpContext->opType = OP_READ_HEAD;
				lpContext->sSocket = lpLink->sSock;
				lpContext->dwByteRecved = 0;
				lpContext->dwBytesToRecv = sizeof(NET_PACKET_HEAD);
				lpContext->wsabuf.buf = (LPSTR)&lpContext->packHead;
				lpContext->wsabuf.len = sizeof(NET_PACKET_HEAD);
				lpContext->lpPacket = nullptr;

				//关联到完成端口
				if (CreateIoCompletionPort((HANDLE)lpLink->sSock, lpThis->m_hIocp, (DWORD)lpLink->sSock, 0) == NULL)
				{
					DbgLog("%s CreateIoCompletionPort error: %d", __FUNCTION__, GetLastError());
					break;
				}

				if (!PostRecv(lpContext))
				{
					break;
				}
				
				CAutoLock lock(&lpThis->m_IoLinkLock);
				lpThis->m_IoLinkList.insert(std::pair<SOCKET, LPIO_CONTEXT>(lpLink->sSock, lpContext));

				InvalidSocket = FALSE;
				DbgView("%s successfully set connect %d from %s", __FUNCTION__, lpLink->sSock, lpLink->PeerAddr.szIP);
			} while (FALSE);

			if (InvalidSocket)
			{
				closesocket(lpLink->sSock);

				if (lpContext != nullptr)
				{
					FreeMemory(lpContext);
				}
			}
			delete lpLink;
		}

		if (lpThis->m_IsStoped)
		{
			DbgView("%s stop event is set, exit now", __FUNCTION__);
			break;
		}	
	}
	return 0;
}

UINT WINAPI CTcpServer::DispatchDataThread(LPVOID lpParam)
{
	CTcpServer *lpThis = (CTcpServer*)lpParam;

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
				lpThis->m_lpListener->OnReceive(lpPack->dwSocket, lpPack->lpBuffer, lpPack->dwTotalLen, 0);
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

UINT WINAPI CTcpServer::WorkerThread(LPVOID lpParam)
{
	CTcpServer *lpThis = (CTcpServer*)lpParam;
	while (!lpThis->m_IsStoped)
	{
		DWORD dwIOSize = 0;
		SOCKET sock = INVALID_SOCKET;
		LPIO_CONTEXT lpContext = NULL;
		if (!GetQueuedCompletionStatus(lpThis->m_hIocp, &dwIOSize, (LPDWORD)&sock, (LPOVERLAPPED*)&lpContext, WSA_INFINITE))
		{
			DWORD dwRet = GetLastError();
			if (dwRet == ERROR_ABANDONED_WAIT_0)
			{
				DbgView("%s completion port handle is closed, exit now", __FUNCTION__);
				break;
			}
			else if (dwRet == ERROR_NETNAME_DELETED)
			{
				//连接已关闭
				lpThis->OnClose(sock);
				continue;
			}
			else
			{
				DbgLog("%s get queued status error: %d", __FUNCTION__, dwRet); 
				continue;
			}
		}
		if (lpThis->m_IsStoped)
		{
			DbgView("%s exited, stop event is set.", __FUNCTION__);
			break;
		}

		if (lpContext->opType == OP_READ_HEAD || lpContext->opType == OP_READ_DATA)
		{
			if (dwIOSize == 0)
			{
				lpThis->OnClose(sock);
			}
			else
			{
				lpThis->OnRecv(lpContext, dwIOSize);
			}
		}
		else if (lpContext->opType == OP_SEND_HEAD || lpContext->opType == OP_SEND_DATA)
		{
			lpThis->OnSend(lpContext, dwIOSize);
		}
	}
	return 0;
}

void CTcpServer::OnRecv(LPIO_CONTEXT lpContext, DWORD dwSize)
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
			lpContext->lpPacket->dwSocket = (DWORD)lpContext->sSocket;
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

void CTcpServer::OnSend(LPIO_CONTEXT lpContext, DWORD dwSize)
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
				m_lpListener->OnSent(lpContext->sSocket, m_SendResult);
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

void CTcpServer::OnClose(SOCKET sock)
{
	if (sock != INVALID_SOCKET)
	{
		closesocket(sock);
	}

	CAutoLock lock(&m_IoLinkLock);
	auto item = m_IoLinkList.find(sock);
	if (item != m_IoLinkList.end())
	{
		LPIO_CONTEXT lpContext = item->second;
		if (lpContext->lpPacket != nullptr)
		{
			FreeMemory(lpContext->lpPacket);
		}
		FreeMemory(lpContext);

		m_IoLinkList.erase(item);

		LPLINK_EVENT lpLink = new LINK_EVENT;
		lpLink->IsConnect = FALSE;
		lpLink->sSock = sock;

		CAutoLock lock(&m_waitLinkLock);
		m_waitLinks.push_back(lpLink);
		ReleaseSemaphore(m_hLinkNotify, 1, nullptr);
	}
}