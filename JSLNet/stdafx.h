// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  �� Windows ͷ�ļ����ų�����ʹ�õ���Ϣ
// Windows ͷ�ļ�:
#include <windows.h>



// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�
#include <process.h>
#include <WinSock2.h>
#include <mstcpip.h>	// ����setsockopt(KEEP_ALIVE)
#pragma comment(lib, "ws2_32.lib")

#include <queue>
#include "../Inc/Lock.h"
#include "../inc/DbgPut.h"