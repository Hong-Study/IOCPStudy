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

//WSAOVERLAPPED구조체를 확장 시켜서 필요한 정보를 더 넣었다.
struct stOverlappedEx
{
	WSAOVERLAPPED	 _wsaOverlapped;	//Overlapped I/O구조체
	WSABUF			_wsaBuf;			//Overlapped I/O작업 버퍼
	EnumOperation	_operation;			//작업 동작 종류
	UINT32			_sessionIndex = 0;
};

#define ClientInfoRef std::shared_ptr<ClientInfo>

#include "ClientInfo.h"