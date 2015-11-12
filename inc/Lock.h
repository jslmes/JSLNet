#pragma once

/** ����һ���������࣬�ṩͬ���ӿ� */
class CLock  
{
public:
	/** Ĭ�Ϲ��캯��������һ�������¼����� */
	CLock()
	{
		//����һ�������¼�����ʼ��״̬Ϊ�ź�̬
		m_hLock = CreateMutex(NULL, FALSE, NULL);
	}

	/** �������һ���Ѵ��ڵ������¼����� */
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

	/** ��ס�����¼����� */
	void Lock()
	{
		WaitForSingleObject(m_hLock, INFINITE);
	}

	void Lock(DWORD dwTime)
	{
		WaitForSingleObject(m_hLock, dwTime);
	}

	/** ���� */
	void UnLock()
	{
		ReleaseMutex(m_hLock);
	}

	/** �ж��Ƿ��ڻ���״̬ */
	BOOL IsLocked()
	{
		DWORD dwRet = WaitForSingleObject(m_hLock, 0);
		if (dwRet == WAIT_OBJECT_0)
		{
			ReleaseMutex(m_hLock); //�����ͷŴ˴εȵ����¼�����
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}

private:
	/** ����ͬ�����¼����� */
	HANDLE m_hLock;
};


//////////////////////////////////////////////////////////////////////////


/** �Զ����࣬�ڹ��캯���м����������������н���
*/
class CAutoLock  
{
public:
	/** ��סCLock���� */
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

	/** �������캯�����ṩ��CLock���� */
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