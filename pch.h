#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <thread>
#include <mutex>
#include <queue>

const UINT32 MAX_SOCKBUF = 1024;
const UINT32 MAX_WORKTHREAD = 4;
const UINT64 RE_USE_SESSION_WAIT_TIMESEC = 3;

enum class EnumOperation
{
	RECV,
	SEND,
	ACCEPT,
	CONNECT,
};

//WSAOVERLAPPED����ü�� Ȯ�� ���Ѽ� �ʿ��� ������ �� �־���.
struct stOverlappedEx
{
	WSAOVERLAPPED	 _wsaOverlapped;	//Overlapped I/O����ü
	WSABUF			_wsaBuf;			//Overlapped I/O�۾� ����
	EnumOperation	_operation;			//�۾� ���� ����
	UINT32			_sessionIndex = 0;
};

#define ClientInfoRef std::shared_ptr<ClientInfo>

#include "ClientInfo.h"