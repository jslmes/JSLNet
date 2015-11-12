#ifndef _JSLNET_2015_11_09_
#define _JSLNET_2015_11_09_


//��������ģ��Ĵ�����
#pragma region error_code

#ifndef JSL_MODULE
//����ģ����ţ�32λ��13-16λ1���ֽ�
#define JSL_MODULE 0x0000
#endif
//�кż�ģ��ţ����ɴ���ŵĸ�5���ֽ�
#define JSL_ERR_BASE	(((__LINE__ & 0xffff)<<16) | JSL_MODULE)

//�ɹ�
#define JSL_ERR_SUCCESS				0
//��ʾ��������
#define JSL_ERR_INVALID_PARAM			(JSL_ERR_BASE + 0x001)
//���治�㣬�޷������ڴ�򴴽���Դ
#define JSL_ERR_OUT_OF_MEMEORY			(JSL_ERR_BASE + 0x002)
//�޷�����socket
#define JSL_ERR_CREATE_SOCKET			(JSL_ERR_BASE + 0x003)
//�޷����ӷ�����
#define JSL_ERR_NET_CONNECT				(JSL_ERR_BASE + 0x004)
//���ݵĻ����С����
#define JSL_ERR_INSUFFICIENT_BUFFER		(JSL_ERR_BASE + 0x005)
//������������ʧ��
#define JSL_ERR_SEND_NET_DATA			(JSL_ERR_BASE + 0x006)
//���������ʱ
#define JSL_ERR_NET_TIMEOUT				(JSL_ERR_BASE + 0x007)
//���ڽ�����������
#define JSL_ERR_NET_BUSY				(JSL_ERR_BASE + 0x008)
//���������ѶϿ�
#define JSL_ERR_NET_DISCONNECTED		(JSL_ERR_BASE + 0x009)

#pragma endregion error_code


#define NET_IP_LEN	18

#pragma pack(1)

/** �������Ӷ˵�ַ */
typedef struct _NET_ADDR_ 
{
	CHAR	szIP[NET_IP_LEN];	//�����ַIP
	USHORT	usPort;				//���Ӷ˿ں�
}NET_ADDR, *LPNET_ADDR;	/** �������Ӷ˵�ַ */
typedef const LPNET_ADDR LPCNET_ADDR;	/** �������Ӷ˵�ַ����ָ�� */

#pragma pack()

/** �����������Ҫʵ�ֵĽӿ� */
class IJslNetListener
{
public:

	/**\brief ��Ϊ������ʱ���յ�һ������ʱ������֪ͨ
	*\param dwNetID ���ӵı�ʶID
	*\param lpPeerAddr ���ӶԷ��ĵ�ַ
	*\param dwErr ���չ����Ƿ�������󣬿ɺ��ԣ�������ճ��������֪ͨ
	*\return ����0��ʾ��ע�����ӣ������������Ч���Ͽ�
	*/
	virtual DWORD OnAccept(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief ��Ϊ�ͻ����������ӵ�һ��������ʱ���߷�������ʼ���������֪ͨ
	*\param dwNetID ���ӵı�ʶID
	*\param lpPeerAddr ���ӶԷ��ĵ�ַ������Ƿ�������ʼ��֪ͨ����ΪNULL
	*\param dwErr �������Ƿ����0��ʾ�ɹ�����
	*/
	virtual DWORD OnConnect(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief ���ӶϿ�ʱ����֪ͨ
	*\param dwNetID ���ӵı�ʶID
	*\param lpPeerAddr ���ӶԷ��ĵ�ַ
	*\param dwErr �Ͽ����ӵĿ���ԭ��
	*/
	virtual DWORD OnClose(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief ���յ���������ʱ��֪ͨ
	*\param dwNetID ���ӵı�ʶID
	*\param lpBuffer ���ݻ�����
	*\param dwBufferLen ���ݳ���
	*\param dwErr �ɺ��ԣ����ճ���������֪ͨ
	*/
	virtual DWORD OnReceive(IN DWORD dwPeerID, IN LPBYTE lpBuffer, IN DWORD dwBufferLen, IN DWORD dwErr) = 0;

	/**\brief �������ݺ��֪ͨ
	*\param dwNetID ����Ŀ������ID
	*\param dwErr ���͹����Ƿ����
	*/
	virtual DWORD OnSent(IN DWORD dwNetID, IN DWORD dwErr) = 0;
};

/** TCP�����ӿ� */
class IJslTcp
{
public:

	/** ɾ���ӿ�ָ�룬�ͷŸ�����Դ */
	virtual DWORD Release() = 0;

	/**\brief ��ʼ������
	*\param bAsServer �Ƿ��ʼ��Ϊ�����
	*\param lpAddr ����Ƿ���ˣ������ṩ�˿ںţ�����ǿͻ��ˣ������ṩ��������IP�Ͷ˿�
	*\param lpListener ���������¼�֪ͨ�Ľӿ�ָ��
	*\return 0��ʾ�ɹ�
	*\note ����˳�ʼ����ʼ�������ӣ��ͻ��˳�ʼ��ʱ����������˵�����
	*/
	virtual DWORD Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener) = 0;

	/** ����ʼ������ */
	virtual DWORD UnInit() = 0;

	/**\brief ͨ��ָ�����ӷ�������
	*\param dwNetID Ŀ�Ķ����ӵ�ID
	*\param lpDataBuf ���ݻ�����
	*\param dwLen ���ݵĳ���
	*\param bAsync ����첽���ͣ����ͽ��ͨ��֪ͨ�ӿڷ��أ����������غ����ݷ��ͳɹ�
	*\return 0��ʾ�ɹ�
	*\note ����Ƿ���������dwConnectID����Ϊ0����ʾ���͵�ÿ���ͻ���
	*/
	virtual DWORD SendData(IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync) = 0;

	/**\brief ��ȡ�������ӵ�ID
	*\param lpIDList ���ID�Ļ�����
	*\param dwCount ID�ĸ�����ͬʱ���Ա�ʾ��Ҫ�������ĳ���
	*\return �ɹ�����0
	*\note ���lpIDListΪNULL����dwCount���ظ������Һ�������0�����lpIDBuffer��ΪNULL����dwCountС��ʵ�ʵĸ�������dwCount���ظ������Һ������ش���
	*/
	virtual DWORD GetConnectList(OUT LPDWORD lpIDList, IN OUT DWORD & dwCount) = 0;

	/**\brief ��ȡĳ������ID����ĵ�ַ��Ϣ
	*\param dwNetID ���ӵ�ID
	*\param lpLocalAddr ���ӵı��ص�ַ��Ϣ
	*\param lpPeerAddr ���ӶԷ��ĵ�ַ��Ϣ
	*\return 0��ʾ�ɹ�
	*/
	virtual DWORD GetConnectInfo(IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr) = 0;

	/** �ر�ĳ�����ӣ������Client�ˣ��൱�ڷ���ʼ���� */
	virtual DWORD Close(DWORD dwNetID) = 0;
};

/**\brief ����TCP�����ӿڵ�ʵ��ʵ��
*\param dwReserved ����ֵ������0
*\param IsServer �Ƿ񴴽�������ָ��
*\param lppJslNet ���ش����ɹ��Ľӿ�ָ��
*\return �ɹ�����0
*\note ������IJslTcpָ����Ҫ����Release��������ɾ��
*/
DWORD WINAPI CreateJslNetPtr(DWORD dwReserved, BOOL IsServer, IJslTcp ** lppJslNet);

/**\brief ��ȡ����ģ����������Ĵ�������
*\param dwErrorCode ������
*\param lpszErrMsg ���������ַ���
*\param iErrLen �����ַ������ȣ�Ӧ����lpszErrMsg�ĳ��ȼ�1
*/
DWORD WINAPI GetJslNetError(DWORD dwErrorCode, LPWSTR lpszErrMsg, int iErrLen);

#endif