#pragma once

#include "JSLNet.h"

#pragma pack(1)

//最大的分包大小，2M
const DWORD MAX_PACKET_LEN = 0x200000;

//数据有效性标志校验值
#define NET_PACKET_HEAD_MARK	0x0103FEA5

//网络数据包的头部
typedef struct _NET_PACKET_HEAD 
{
	//数据有效性标志
	DWORD	dwMark;
	//数据的长度，不包括头部
	DWORD	dwDataLen;
}NET_PACKET_HEAD, *LPNET_PACKET_HEAD;

//接收或发送，且经过整合后的数据包
typedef struct _NET_PACKET 
{
	//SOCKET表示的ID
	DWORD dwSocket;
	//总的数据大小(字节)
	DWORD dwTotalLen;
	//已发送或接收的数据大小
	DWORD dwProcessed;
	//待处理的数据缓冲区
	BYTE lpBuffer[1];
}NET_PACKET, *LPNET_PACKET;

//操作类型
typedef enum _OP_TYPE_ 
{
	OP_READ_HEAD,	//正在接收头部
	OP_READ_DATA,	//正在接收数据
	OP_SEND_HEAD,	//正在发送头部
	OP_SEND_DATA	//同步发送数据
}OP_TYPE;

/** 描述一次发送或接收的数据结构 */
typedef struct _IO_CONTEXT_
{
	//重叠结构
	WSAOVERLAPPED Overlapped;		
	//投递某个IO请求的Socket
	SOCKET sSocket;				
	//缓冲区, 包括数据指针和长度
	WSABUF wsabuf;	
	//本次发送或接收的数据总长度
	DWORD dwBytesToRecv;
	//已经接收或发送的数据长度
	DWORD dwByteRecved;
	//操作类型
	OP_TYPE opType;
	//包头部
	NET_PACKET_HEAD packHead;
	//当前正在操作的数据
	LPNET_PACKET	lpPacket;
}IO_CONTEXT, *LPIO_CONTEXT;

//服务器、客户端的连接
typedef struct _LINK_EVENT 
{
	SOCKET sSock;
	//是否是连接事件，还是断开事件
	BOOL IsConnect;
	//连接对方的IP地址
	NET_ADDR PeerAddr;
}LINK_EVENT, *LPLINK_EVENT;

#pragma pack()

// 在自定义堆上分配内存, 缺省堆出问题时希望不影响本模块
extern HANDLE g_hHeap;
#define AllocMemory(SIZE)         HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, SIZE)
#define FreeMemory(P)             HeapFree(g_hHeap, 0, P)


/** 设置KeepAlive探测 */
void SetKeepAlive(SOCKET sock);

/** 等待并关闭线程 */
void CloseThread(HANDLE * lpThread, DWORD dwSeconds);

/** 获取Socket关联的本地和远程地址 */
BOOL GetSocketAddr(SOCKET sock, LPNET_ADDR lpLocal, LPNET_ADDR lpRemote);

//关闭连接
void SafeCloseSocket(SOCKET & sock);

BOOL PostRecv(LPIO_CONTEXT pContext);

BOOL PostSend(LPIO_CONTEXT pContext);