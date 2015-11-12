#pragma once

#include "jslnet.h"
#include "Utils.h"

class CTcpClient : public IJslTcp
{
public:

	CTcpClient(void);

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

	void OnRecv(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnSend(LPIO_CONTEXT lpContext, DWORD dwSize);

	void OnClose(DWORD dwErr);

private:

	~CTcpClient(void);

	//�¼��ȴ��߳�
	static UINT WINAPI WorkerThread(LPVOID lpParam);

	//���ݷַ��߳�
	static UINT WINAPI DispatchThread(LPVOID lpParam);

	//�����̳߳أ���������֪ͨ
	static DWORD WINAPI FireConnectEvent(LPVOID lpParam);

	//�����̳߳أ��������ӶϿ�֪ͨ
	static DWORD WINAPI FireCloseEvent(LPVOID lpParam);

private:

	//�����Ƿ���ֹͣ
	BOOL m_IsStoped;

	//��������ַ
	NET_ADDR m_svrAddr;
	//����������ӵ��׽���
	SOCKET m_svrSock;
	//������ʵ�ֽӿ�
	IJslNetListener *m_lpListener;

	//0:���¼���1:д�¼���2:�˳��¼�
	WSAEVENT m_events[3];
	//��д�ȴ��߳�
	HANDLE m_hWorkThread;

	//��ǰ���ڽ��յ����ݰ�
	IO_CONTEXT m_RecvingData;
	//��ǰ���ڷ��͵����ݰ�
	IO_CONTEXT m_SendingData;
	//�������֪ͨ�¼�
	WSAEVENT m_SentCompleted;
	//���ͽ��
	DWORD m_SendResult;

	//�����������ݱ�Ļ�����
	CLock m_RecvListLock;
	//���յ������б�
	std::queue<LPNET_PACKET> m_RecvList;
	//�ַ��������ݵĶ����߳�
	HANDLE m_hIoThread;
	//���ݵ���֪ͨ
	HANDLE m_hIoNotify;
};

