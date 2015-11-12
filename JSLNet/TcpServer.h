#pragma once

#include "jslnet.h"
#include "Utils.h"

#include <vector>
#include <map>

class CTcpServer : public IJslTcp
{
public:
	CTcpServer(void);

	/** 删除接口指针，释放各种资源 */
	virtual DWORD Release();

	/**\brief 初始化网络
	*\param lpAddr 如果是服务端，仅需提供端口号，如果是客户端，则需提供服务器的IP和端口
	*\param lpListener 接收网络事件通知的接口指针
	*\return 0表示成功
	*\note 服务端初始化后开始接收连接，客户端初始化时建立到服务端的连接
	*/
	virtual DWORD Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener );

	/** 反初始化网络 */
	virtual DWORD UnInit();

	/**\brief 通过指定连接发送数据
	*\param dwNetID 目的端连接的ID
	*\param lpDataBuf 数据缓冲区
	*\param dwLen 数据的长度
	*\param bAsync 如果异步发送，发送结果通过通知接口返回，否则函数返回后数据发送成功
	*\return 0表示成功
	*\note 如果是服务器，则dwConnectID可以为0，表示发送到每个客户端
	*/
	virtual DWORD SendData( IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync );

	/**\brief 获取所有连接的ID
	*\param lpIDList 存放ID的缓冲区
	*\param dwCount ID的个数，同时可以表示需要缓冲区的长度
	*\return 成功返回0
	*\note 如果lpIDList为NULL，则dwCount返回个数并且函数返回0，如果lpIDBuffer不为NULL，而dwCount小于实际的个数，则dwCount返回个数并且函数返回错误
	*/
	virtual DWORD GetConnectList( OUT LPDWORD lpIDList, IN OUT DWORD & dwCount );

	/**\brief 获取某个连接ID代表的地址信息
	*\param dwNetID 连接的ID
	*\param lpLocalAddr 连接的本地地址信息
	*\param lpPeerAddr 连接对方的地址信息
	*\return 0表示成功
	*/
	virtual DWORD GetConnectInfo( IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr );

	/** 关闭某个连接，如果是Client端，相当于反初始化了 */
	virtual DWORD Close( DWORD dwNetID );

private:

	~CTcpServer(void);

	//等待连接线程
	static UINT WINAPI AcceptThread(LPVOID lpParam);

	//数据分发线程
	static UINT WINAPI DispatchDataThread(LPVOID lpParam);

	//连接到达通知线程
	static UINT WINAPI DispatchLinkThread(LPVOID lpParam);

	//完成端口线程
	static UINT WINAPI WorkerThread(LPVOID lpParam);

	//产生初始化完成的通知
	static DWORD WINAPI FireInitedEvent(LPVOID lpThreadParameter);

	void OnRecv(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnSend(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnClose(SOCKET sock);

private:

	//监听者实现接口
	IJslNetListener *m_lpListener;

	//服务是否停止
	BOOL m_IsStoped;
	//服务器监听SOCKET
	SOCKET m_svrSock;
	//等待连接线程
	HANDLE m_hAcceptThread;
	//等待连接线程事件，0：接收连接,1：退出事件
	WSAEVENT m_acceptEvents[2];
	//连接通知线程
	HANDLE m_hLinkThread;
	//等待通知上层的连接链表保护互斥量
	CLock m_waitLinkLock;
	//等待通知上层的连接链表
	std::vector<LPLINK_EVENT> m_waitLinks;
	//连接到达通知
	HANDLE m_hLinkNotify;

	//完成端口的句柄
	HANDLE m_hIocp;
	//线程池句柄
	std::vector<HANDLE> m_threadPool;

	//保护数据连接表的互斥量
	CLock m_IoLinkLock;
	//数据连接链表
	std::map<SOCKET, LPIO_CONTEXT> m_IoLinkList;

	//保护接收数据表的互斥量
	CLock m_RecvListLock;
	//接收到数据列表
	std::queue<LPNET_PACKET> m_RecvList;
	//分发接收数据的独立线程
	HANDLE m_hIoThread;
	//数据到达通知
	HANDLE m_hIoNotify;

	//当前正在发送的数据包
	IO_CONTEXT m_SendingData;
	//发送完成通知事件
	WSAEVENT m_SentCompleted;
	//发送结果
	DWORD m_SendResult;
};

