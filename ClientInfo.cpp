#include "pch.h"
#include "ClientInfo.h"

ClientInfo::ClientInfo() : 
	_acceptBuf(std::array<char, 64>()), _recvBuf(std::array<char, MAX_SOCKBUF>())
{
	ZeroMemory(&_recvOverlappedEx, sizeof(stOverlappedEx));
	_socket = INVALID_SOCKET;
}

void ClientInfo::Init(const UINT32 index, HANDLE iocpHandle)
{
	_index = index;
	_iocpHandle = iocpHandle;
}

bool ClientInfo::OnConnect(HANDLE iocpHandle, SOCKET socket)
{
	_socket = socket;
	_isConnect = 1;

	Clear();

	if (BindIOCompletionPort(iocpHandle) == false)
	{
		return false;
	}

	return BindRecv();
}

bool ClientInfo::BindIOCompletionPort(HANDLE iocpHandle)
{
	_iocpHandle = iocpHandle;

	//socket�� pClientInfo�� CompletionPort��ü�� �����Ų��.
	auto iocpRet = CreateIoCompletionPort((HANDLE)GetSock(), _iocpHandle, (ULONG_PTR)(this), 0);

	if (iocpRet == INVALID_HANDLE_VALUE)
	{
		printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
		return false;
	}

	return true;
}

void ClientInfo::Close(bool isForce)
{
	struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

	// bIsForce�� true�̸� SO_LINGER, timeout = 0���� �����Ͽ� ���� ���� ��Ų��. ���� : ������ �ս��� ������ ���� 
	if (true == isForce)
	{
		stLinger.l_onoff = 1;
	}

	//socketClose������ ������ �ۼ����� ��� �ߴ� ��Ų��.
	shutdown(_socket, SD_BOTH);

	//���� �ɼ��� �����Ѵ�.
	setsockopt(_socket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

	_isConnect = 0;
	_latestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		//���� ������ ���� ��Ų��.
		closesocket(_socket);
	_socket = INVALID_SOCKET;
}

bool ClientInfo::PostAccept(SOCKET listenSock, const UINT64 curTimeSec)
{
	printf_s("PostAccept. client Index: %d\n", GetIndex());

	_latestClosedTimeSec = UINT32_MAX;

	_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == _socket)
	{
		printf_s("client Socket WSASocket Error : %d\n", GetLastError());
		return false;
	}

	ZeroMemory(&_acceptContext, sizeof(stOverlappedEx));

	DWORD bytes = 0;

	_acceptContext._wsaBuf.len = 0;
	_acceptContext._wsaBuf.buf = nullptr;
	_acceptContext._operation = EnumOperation::ACCEPT;
	_acceptContext._sessionIndex = _index;

	if (FALSE == AcceptEx(listenSock, _socket, _acceptBuf.data(), 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (_acceptContext)))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf_s("AcceptEx Error : %d\n", GetLastError());
			return false;
		}
	}

	return true;
}

bool ClientInfo::AcceptCompletion()
{
	printf_s("AcceptCompletion : SessionIndex(%d)\n", _index);

	if (OnConnect(_iocpHandle, _socket) == false)
	{
		return false;
	}

	SOCKADDR_IN		clientAddr;

	char clientIP[32] = { 0, };
	inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, 32 - 1);
	printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)_socket);

	return true;
}

void ClientInfo::SendCompletion(const UINT32 dataSize)
{
	printf("[�۽� �Ϸ�] bytes : %d\n", dataSize);

	std::lock_guard<std::mutex> guard(_sendLock);

	delete[] _sendQueue.front()->_wsaBuf.buf;
	delete _sendQueue.front();

	_sendQueue.pop();

	if (_sendQueue.empty() == false)
	{
		BindSend();
	}
}

bool ClientInfo::SendMsg(const UINT32 size, char* data)
{
	auto sendOverlappedEx = new stOverlappedEx();
	ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));

	sendOverlappedEx->_wsaBuf.buf = new char[size];
	sendOverlappedEx->_wsaBuf.len = size;
	sendOverlappedEx->_operation = EnumOperation::SEND;
	sendOverlappedEx->_sessionIndex = _index;

	CopyMemory(sendOverlappedEx->_wsaBuf.buf, data, size);

	std::lock_guard<std::mutex> lock(_sendLock);

	_sendQueue.push(sendOverlappedEx);

	if (_sendQueue.size() == 1)
	{
		BindSend();
	}

	return true;
}

bool ClientInfo::BindRecv()
{
	DWORD flags = 0;
	DWORD recvNumBytes = 0;

	_recvOverlappedEx._wsaBuf.buf = _recvBuf.data();
	_recvOverlappedEx._wsaBuf.len = MAX_SOCKBUF;
	_recvOverlappedEx._operation = EnumOperation::RECV;
	_recvOverlappedEx._sessionIndex = _index;

	int ret = WSARecv(_socket, &(_recvOverlappedEx._wsaBuf), 1, &recvNumBytes, &flags, (LPWSAOVERLAPPED)&_recvOverlappedEx, nullptr);

	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		printf_s("[����] WSARecv Error : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool ClientInfo::BindSend()
{
	auto sendOverlappedEx = _sendQueue.front();

	DWORD dwRecvNumBytes = 0;
	int nRet = WSASend(_socket,
		&(sendOverlappedEx->_wsaBuf),
		1,
		&dwRecvNumBytes,
		0,
		(LPWSAOVERLAPPED)sendOverlappedEx,
		NULL);

	//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}


