#pragma once

#include "JSLNet.h"

#pragma pack(1)

//���ķְ���С��2M
const DWORD MAX_PACKET_LEN = 0x200000;

//������Ч�Ա�־У��ֵ
#define NET_PACKET_HEAD_MARK	0x0103FEA5

//�������ݰ���ͷ��
typedef struct _NET_PACKET_HEAD 
{
	//������Ч�Ա�־
	DWORD	dwMark;
	//���ݵĳ��ȣ�������ͷ��
	DWORD	dwDataLen;
}NET_PACKET_HEAD, *LPNET_PACKET_HEAD;

//���ջ��ͣ��Ҿ������Ϻ�����ݰ�
typedef struct _NET_PACKET 
{
	//SOCKET��ʾ��ID
	DWORD dwSocket;
	//�ܵ����ݴ�С(�ֽ�)
	DWORD dwTotalLen;
	//�ѷ��ͻ���յ����ݴ�С
	DWORD dwProcessed;
	//����������ݻ�����
	BYTE lpBuffer[1];
}NET_PACKET, *LPNET_PACKET;

//��������
typedef enum _OP_TYPE_ 
{
	OP_READ_HEAD,	//���ڽ���ͷ��
	OP_READ_DATA,	//���ڽ�������
	OP_SEND_HEAD,	//���ڷ���ͷ��
	OP_SEND_DATA	//ͬ����������
}OP_TYPE;

/** ����һ�η��ͻ���յ����ݽṹ */
typedef struct _IO_CONTEXT_
{
	//�ص��ṹ
	WSAOVERLAPPED Overlapped;		
	//Ͷ��ĳ��IO�����Socket
	SOCKET sSocket;				
	//������, ��������ָ��ͳ���
	WSABUF wsabuf;	
	//���η��ͻ���յ������ܳ���
	DWORD dwBytesToRecv;
	//�Ѿ����ջ��͵����ݳ���
	DWORD dwByteRecved;
	//��������
	OP_TYPE opType;
	//��ͷ��
	NET_PACKET_HEAD packHead;
	//��ǰ���ڲ���������
	LPNET_PACKET	lpPacket;
}IO_CONTEXT, *LPIO_CONTEXT;

//���������ͻ��˵�����
typedef struct _LINK_EVENT 
{
	SOCKET sSock;
	//�Ƿ��������¼������ǶϿ��¼�
	BOOL IsConnect;
	//���ӶԷ���IP��ַ
	NET_ADDR PeerAddr;
}LINK_EVENT, *LPLINK_EVENT;

#pragma pack()

// ���Զ�����Ϸ����ڴ�, ȱʡ�ѳ�����ʱϣ����Ӱ�챾ģ��
extern HANDLE g_hHeap;
#define AllocMemory(SIZE)         HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, SIZE)
#define FreeMemory(P)             HeapFree(g_hHeap, 0, P)


/** ����KeepAlive̽�� */
void SetKeepAlive(SOCKET sock);

/** �ȴ����ر��߳� */
void CloseThread(HANDLE * lpThread, DWORD dwSeconds);

/** ��ȡSocket�����ı��غ�Զ�̵�ַ */
BOOL GetSocketAddr(SOCKET sock, LPNET_ADDR lpLocal, LPNET_ADDR lpRemote);

//�ر�����
void SafeCloseSocket(SOCKET & sock);

BOOL PostRecv(LPIO_CONTEXT pContext);

BOOL PostSend(LPIO_CONTEXT pContext);