#pragma once

#include "jslnet.h"
#include "Utils.h"

#include <vector>
#include <map>

class CTcpServer : public IJslTcp
{
public:
	CTcpServer(void);

	/** ɾ���ӿ�ָ�룬�ͷŸ�����Դ */
	virtual DWORD Release();

	/**\brief ��ʼ������
	*\param lpAddr ����Ƿ���ˣ������ṩ�˿ںţ�����ǿͻ��ˣ������ṩ��������IP�Ͷ˿�
	*\param lpListener ���������¼�֪ͨ�Ľӿ�ָ��
	*\return 0��ʾ�ɹ�
	*\note ����˳�ʼ����ʼ�������ӣ��ͻ��˳�ʼ��ʱ����������˵�����
	*/
	virtual DWORD Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener );

	/** ����ʼ������ */
	virtual DWORD UnInit();

	/**\brief ͨ��ָ�����ӷ�������
	*\param dwNetID Ŀ�Ķ����ӵ�ID
	*\param lpDataBuf ���ݻ�����
	*\param dwLen ���ݵĳ���
	*\param bAsync ����첽���ͣ����ͽ��ͨ��֪ͨ�ӿڷ��أ����������غ����ݷ��ͳɹ�
	*\return 0��ʾ�ɹ�
	*\note ����Ƿ���������dwConnectID����Ϊ0����ʾ���͵�ÿ���ͻ���
	*/
	virtual DWORD SendData( IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync );

	/**\brief ��ȡ�������ӵ�ID
	*\param lpIDList ���ID�Ļ�����
	*\param dwCount ID�ĸ�����ͬʱ���Ա�ʾ��Ҫ�������ĳ���
	*\return �ɹ�����0
	*\note ���lpIDListΪNULL����dwCount���ظ������Һ�������0�����lpIDBuffer��ΪNULL����dwCountС��ʵ�ʵĸ�������dwCount���ظ������Һ������ش���
	*/
	virtual DWORD GetConnectList( OUT LPDWORD lpIDList, IN OUT DWORD & dwCount );

	/**\brief ��ȡĳ������ID����ĵ�ַ��Ϣ
	*\param dwNetID ���ӵ�ID
	*\param lpLocalAddr ���ӵı��ص�ַ��Ϣ
	*\param lpPeerAddr ���ӶԷ��ĵ�ַ��Ϣ
	*\return 0��ʾ�ɹ�
	*/
	virtual DWORD GetConnectInfo( IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr );

	/** �ر�ĳ�����ӣ������Client�ˣ��൱�ڷ���ʼ���� */
	virtual DWORD Close( DWORD dwNetID );

private:

	~CTcpServer(void);

	//�ȴ������߳�
	static UINT WINAPI AcceptThread(LPVOID lpParam);

	//���ݷַ��߳�
	static UINT WINAPI DispatchDataThread(LPVOID lpParam);

	//���ӵ���֪ͨ�߳�
	static UINT WINAPI DispatchLinkThread(LPVOID lpParam);

	//��ɶ˿��߳�
	static UINT WINAPI WorkerThread(LPVOID lpParam);

	//������ʼ����ɵ�֪ͨ
	static DWORD WINAPI FireInitedEvent(LPVOID lpThreadParameter);

	void OnRecv(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnSend(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnClose(SOCKET sock);

private:

	//������ʵ�ֽӿ�
	IJslNetListener *m_lpListener;

	//�����Ƿ�ֹͣ
	BOOL m_IsStoped;
	//����������SOCKET
	SOCKET m_svrSock;
	//�ȴ������߳�
	HANDLE m_hAcceptThread;
	//�ȴ������߳��¼���0����������,1���˳��¼�
	WSAEVENT m_acceptEvents[2];
	//����֪ͨ�߳�
	HANDLE m_hLinkThread;
	//�ȴ�֪ͨ�ϲ������������������
	CLock m_waitLinkLock;
	//�ȴ�֪ͨ�ϲ����������
	std::vector<LPLINK_EVENT> m_waitLinks;
	//���ӵ���֪ͨ
	HANDLE m_hLinkNotify;

	//��ɶ˿ڵľ��
	HANDLE m_hIocp;
	//�̳߳ؾ��
	std::vector<HANDLE> m_threadPool;

	//�����������ӱ�Ļ�����
	CLock m_IoLinkLock;
	//������������
	std::map<SOCKET, LPIO_CONTEXT> m_IoLinkList;

	//�����������ݱ�Ļ�����
	CLock m_RecvListLock;
	//���յ������б�
	std::queue<LPNET_PACKET> m_RecvList;
	//�ַ��������ݵĶ����߳�
	HANDLE m_hIoThread;
	//���ݵ���֪ͨ
	HANDLE m_hIoNotify;

	//��ǰ���ڷ��͵����ݰ�
	IO_CONTEXT m_SendingData;
	//�������֪ͨ�¼�
	WSAEVENT m_SentCompleted;
	//���ͽ��
	DWORD m_SendResult;
};

