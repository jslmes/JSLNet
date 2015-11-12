#pragma once

/** 定义一个互斥锁类，提供同步接口 */
class CLock  
{
public:
	/** 默认构造函数，创建一个无名事件对象 */
	CLock()
	{
		//创建一个互斥事件，初始化状态为信号态
		m_hLock = CreateMutex(NULL, FALSE, NULL);
	}

	/** 创建或打开一个已存在的命名事件对象 */
	CLock(const TCHAR *tcsEventName)
	{
		m_hLock = CreateMutex(NULL, FALSE, tcsEventName);
		if (m_hLock == NULL && GetLastError() == ERROR_ALREADY_EXISTS)
		{
			m_hLock = OpenMutex(EVENT_ALL_ACCESS , TRUE, tcsEventName);
		}
	}

	virtual ~CLock()
	{
		if (m_hLock != NULL)
		{
			CloseHandle(m_hLock);
			m_hLock = NULL;
		}
	}

	/** 锁住互斥事件对象 */
	void Lock()
	{
		WaitForSingleObject(m_hLock, INFINITE);
	}

	void Lock(DWORD dwTime)
	{
		WaitForSingleObject(m_hLock, dwTime);
	}

	/** 解锁 */
	void UnLock()
	{
		ReleaseMutex(m_hLock);
	}

	/** 判断是否处于互斥状态 */
	BOOL IsLocked()
	{
		DWORD dwRet = WaitForSingleObject(m_hLock, 0);
		if (dwRet == WAIT_OBJECT_0)
		{
			ReleaseMutex(m_hLock); //重新释放此次等到的事件对象
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}

private:
	/** 用于同步的事件对象 */
	HANDLE m_hLock;
};


//////////////////////////////////////////////////////////////////////////


/** 自动锁类，在构造函数中加锁，在析构函数中解锁
*/
class CAutoLock  
{
public:
	/** 锁住CLock对象 */
	CAutoLock(CLock *pLock)
	{
		if (pLock != NULL)
		{
			m_pLock = pLock;
			m_pLock->Lock();
		}
	}

	CAutoLock(CLock *cLock, DWORD dwTime)
	{
		if (cLock != NULL)
		{
			m_pLock = cLock;
			m_pLock->Lock(dwTime);
		}
	}

	/** 解锁构造函数中提供的CLock对象 */
	virtual ~CAutoLock()
	{
		if (m_pLock != NULL)
		{
			m_pLock->UnLock();
			m_pLock = NULL;
		}
	}

private:
	CLock *m_pLock;
};