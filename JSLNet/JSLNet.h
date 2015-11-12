#ifndef _JSLNET_2015_11_09_
#define _JSLNET_2015_11_09_


//定义网络模块的错误码
#pragma region error_code

#ifndef JSL_MODULE
//程序模块代号，32位的13-16位1个字节
#define JSL_MODULE 0x0000
#endif
//行号加模块号，构成错误号的高5个字节
#define JSL_ERR_BASE	(((__LINE__ & 0xffff)<<16) | JSL_MODULE)

//成功
#define JSL_ERR_SUCCESS				0
//表示参数错误
#define JSL_ERR_INVALID_PARAM			(JSL_ERR_BASE + 0x001)
//缓存不足，无法申请内存或创建资源
#define JSL_ERR_OUT_OF_MEMEORY			(JSL_ERR_BASE + 0x002)
//无法创建socket
#define JSL_ERR_CREATE_SOCKET			(JSL_ERR_BASE + 0x003)
//无法连接服务器
#define JSL_ERR_NET_CONNECT				(JSL_ERR_BASE + 0x004)
//传递的缓存大小不够
#define JSL_ERR_INSUFFICIENT_BUFFER		(JSL_ERR_BASE + 0x005)
//发送网络数据失败
#define JSL_ERR_SEND_NET_DATA			(JSL_ERR_BASE + 0x006)
//网络操作超时
#define JSL_ERR_NET_TIMEOUT				(JSL_ERR_BASE + 0x007)
//正在进行其他操作
#define JSL_ERR_NET_BUSY				(JSL_ERR_BASE + 0x008)
//网络连接已断开
#define JSL_ERR_NET_DISCONNECTED		(JSL_ERR_BASE + 0x009)

#pragma endregion error_code


#define NET_IP_LEN	18

#pragma pack(1)

/** 网络连接端地址 */
typedef struct _NET_ADDR_ 
{
	CHAR	szIP[NET_IP_LEN];	//网络地址IP
	USHORT	usPort;				//连接端口号
}NET_ADDR, *LPNET_ADDR;	/** 网络连接端地址 */
typedef const LPNET_ADDR LPCNET_ADDR;	/** 网络连接端地址常量指针 */

#pragma pack()

/** 网络监听者需要实现的接口 */
class IJslNetListener
{
public:

	/**\brief 作为服务器时接收到一个连接时产生的通知
	*\param dwNetID 连接的标识ID
	*\param lpPeerAddr 连接对方的地址
	*\param dwErr 接收过程是否产生错误，可忽略，如果接收出错不会产生通知
	*\return 返回0表示关注此连接，否则此连接无效，断开
	*/
	virtual DWORD OnAccept(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief 作为客户端主动连接到一个服务器时或者服务器初始化后产生此通知
	*\param dwNetID 连接的标识ID
	*\param lpPeerAddr 连接对方的地址，如果是服务器初始化通知，则为NULL
	*\param dwErr 此连接是否出错，0表示成功连接
	*/
	virtual DWORD OnConnect(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief 连接断开时产生通知
	*\param dwNetID 连接的标识ID
	*\param lpPeerAddr 连接对方的地址
	*\param dwErr 断开连接的可能原因
	*/
	virtual DWORD OnClose(IN DWORD dwNetID, IN LPCNET_ADDR lpPeerAddr, IN DWORD dwErr) = 0;

	/**\brief 接收到网络数据时的通知
	*\param dwNetID 连接的标识ID
	*\param lpBuffer 数据缓冲区
	*\param dwBufferLen 数据长度
	*\param dwErr 可忽略，接收出错不产生此通知
	*/
	virtual DWORD OnReceive(IN DWORD dwPeerID, IN LPBYTE lpBuffer, IN DWORD dwBufferLen, IN DWORD dwErr) = 0;

	/**\brief 发送数据后的通知
	*\param dwNetID 数据目的连接ID
	*\param dwErr 发送过程是否出错
	*/
	virtual DWORD OnSent(IN DWORD dwNetID, IN DWORD dwErr) = 0;
};

/** TCP操作接口 */
class IJslTcp
{
public:

	/** 删除接口指针，释放各种资源 */
	virtual DWORD Release() = 0;

	/**\brief 初始化网络
	*\param bAsServer 是否初始化为服务端
	*\param lpAddr 如果是服务端，仅需提供端口号，如果是客户端，则需提供服务器的IP和端口
	*\param lpListener 接收网络事件通知的接口指针
	*\return 0表示成功
	*\note 服务端初始化后开始接收连接，客户端初始化时建立到服务端的连接
	*/
	virtual DWORD Init(IN LPCNET_ADDR lpAddr, IN IJslNetListener *lpListener) = 0;

	/** 反初始化网络 */
	virtual DWORD UnInit() = 0;

	/**\brief 通过指定连接发送数据
	*\param dwNetID 目的端连接的ID
	*\param lpDataBuf 数据缓冲区
	*\param dwLen 数据的长度
	*\param bAsync 如果异步发送，发送结果通过通知接口返回，否则函数返回后数据发送成功
	*\return 0表示成功
	*\note 如果是服务器，则dwConnectID可以为0，表示发送到每个客户端
	*/
	virtual DWORD SendData(IN DWORD dwNetID, IN LPBYTE lpDataBuf, IN DWORD dwLen, IN BOOL bAsync) = 0;

	/**\brief 获取所有连接的ID
	*\param lpIDList 存放ID的缓冲区
	*\param dwCount ID的个数，同时可以表示需要缓冲区的长度
	*\return 成功返回0
	*\note 如果lpIDList为NULL，则dwCount返回个数并且函数返回0，如果lpIDBuffer不为NULL，而dwCount小于实际的个数，则dwCount返回个数并且函数返回错误
	*/
	virtual DWORD GetConnectList(OUT LPDWORD lpIDList, IN OUT DWORD & dwCount) = 0;

	/**\brief 获取某个连接ID代表的地址信息
	*\param dwNetID 连接的ID
	*\param lpLocalAddr 连接的本地地址信息
	*\param lpPeerAddr 连接对方的地址信息
	*\return 0表示成功
	*/
	virtual DWORD GetConnectInfo(IN DWORD dwNetID, OUT LPNET_ADDR lpLocalAddr, OUT LPNET_ADDR lpPeerAddr) = 0;

	/** 关闭某个连接，如果是Client端，相当于反初始化了 */
	virtual DWORD Close(DWORD dwNetID) = 0;
};

/**\brief 创建TCP操作接口的实现实例
*\param dwReserved 保留值，传入0
*\param IsServer 是否创建服务器指针
*\param lppJslNet 返回创建成功的接口指针
*\return 成功返回0
*\note 创建的IJslTcp指针需要调用Release方法返回删除
*/
DWORD WINAPI CreateJslNetPtr(DWORD dwReserved, BOOL IsServer, IJslTcp ** lppJslNet);

/**\brief 获取网络模块错误码代表的错误描述
*\param dwErrorCode 错误码
*\param lpszErrMsg 错误描述字符串
*\param iErrLen 最大的字符串长度，应该是lpszErrMsg的长度减1
*/
DWORD WINAPI GetJslNetError(DWORD dwErrorCode, LPWSTR lpszErrMsg, int iErrLen);

#endif