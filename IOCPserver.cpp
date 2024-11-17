#include "pch.h"
#include "IOCPserver.h"

bool IOCPserver::Init(const UINT32 maxThreadCount)
{
	WSADATA wsaData;

	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != nRet)
	{
		printf("[����] WSAStartup()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	//���������� TCP , Overlapped I/O ������ ����
	_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == _listenSocket)
	{
		printf("[����] socket()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	_maxWorkerThreadCount = maxThreadCount;

	printf("���� �ʱ�ȭ ����\n");
	return true;
}

bool IOCPserver::BindandListen(short bindPort, int acceptIp)
{
	SOCKADDR_IN		stServerAddr;
	stServerAddr.sin_family = AF_INET;
	stServerAddr.sin_port = htons(bindPort); //���� ��Ʈ�� �����Ѵ�.		
	//� �ּҿ��� ������ �����̶� �޾Ƶ��̰ڴ�.
	//���� ������� �̷��� �����Ѵ�. ���� �� �����ǿ����� ������ �ް� �ʹٸ�
	//�� �ּҸ� inet_addr�Լ��� �̿��� ������ �ȴ�.
	stServerAddr.sin_addr.s_addr = htonl(acceptIp);

	//������ ������ ���� �ּ� ������ cIOCompletionPort ������ �����Ѵ�.
	int nRet = bind(_listenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
	if (0 != nRet)
	{
		printf("[����] bind()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	//���� ��û�� �޾Ƶ��̱� ���� cIOCompletionPort������ ����ϰ� 
	//���Ӵ��ť�� 5���� ���� �Ѵ�.
	nRet = listen(_listenSocket, 5);
	if (0 != nRet)
	{
		printf("[����] listen()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	//CompletionPort��ü ���� ��û�� �Ѵ�.
	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, _maxWorkerThreadCount);
	if (NULL == _iocpHandle)
	{
		printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
		return false;
	}

	auto hIOCPHandle = CreateIoCompletionPort((HANDLE)_listenSocket, _iocpHandle, (UINT32)0, 0);
	if (nullptr == hIOCPHandle)
	{
		printf("[����] listen socket IOCP bind ���� : %d\n", WSAGetLastError());
		return false;
	}

	printf("���� ��� ����..\n");
	return true;
}

bool IOCPserver::StartServer(const UINT32 maxClientCount)
{
	CreateClient(maxClientCount);

	//���ӵ� Ŭ���̾�Ʈ �ּ� ������ ������ ����ü
	bool bRet = CreateWorkerThread();
	if (false == bRet) {
		return false;
	}

	bRet = CreateAcceptThread();
	if (false == bRet) {
		return false;
	}

	printf("���� ����\n");
	return true;
}

void IOCPserver::StopServer()
{
	_isWorkerRun = false;
	_isAcceptRun = false;
	CloseHandle(_iocpHandle);

	for (auto& th : _workerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	closesocket(_listenSocket);

	if (_acceptThread.joinable())
	{
		_acceptThread.join();
	}
}

bool IOCPserver::SendMsg(UINT32 clientIndex, UINT32 size, char* data)
{
	auto pClient = GetClientInfoRef(clientIndex);
	return pClient->SendMsg(size, data);
}


void IOCPserver::CreateClient(const UINT32 maxClientCount)
{
	for (UINT32 i = 0; i < maxClientCount; i++)
	{
		auto client = std::make_shared<ClientInfo>();
		client->Init(i, _iocpHandle);

		_clientInfos.push_back(client);
	}
}

ClientInfoRef IOCPserver::GetEmptyClientInfoRef()
{
	for (auto& client : _clientInfos)
	{
		if (client->IsConnectd() == false)
		{
			return client;
		}
	}

	return nullptr;
}

ClientInfoRef IOCPserver::GetClientInfoRef(const UINT32 index)
{
	if (_clientInfos.size() <= index)
	{
		return nullptr;
	}

	return _clientInfos[index];
 }

bool IOCPserver::CreateWorkerThread()
{
	for (UINT32 i = 0; i < _maxWorkerThreadCount; i++)
	{
		_workerThreads.emplace_back([this]() { WorkerThread(); });
	}

	printf("WokerThread ����..\n");
	return true;
}

bool IOCPserver::CreateAcceptThread()
{
	_acceptThread = std::thread([this]() { AcceptThread(); });

	printf("AccepterThread ����..\n");
	return true;
}

void IOCPserver::CloseSocket(UINT32 clientIndex, bool bIsForce)
{
	auto clientInfo = GetClientInfoRef(clientIndex);
	if (clientInfo == nullptr)
	{
		return;
	}

	if (clientInfo->IsConnectd() == false)
	{
		return;
	}

	clientInfo->Close(bIsForce);

	OnClose(clientInfo->GetIndex());
}

void IOCPserver::WorkerThread()
{
	ULONG_PTR key;
	//�Լ� ȣ�� ���� ����
	BOOL bSuccess = TRUE;
	//Overlapped I/O�۾����� ���۵� ������ ũ��
	DWORD dwIoSize = 0;
	//I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
	LPOVERLAPPED lpOverlapped = NULL;

	while (_isWorkerRun)
	{
		bSuccess = GetQueuedCompletionStatus(
			_iocpHandle,
			&dwIoSize,
			(PULONG_PTR)&key,
			(LPOVERLAPPED*)&lpOverlapped,
			INFINITE);

		// ����� ������ ���� �޽��� ó��
		if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped)
		{
			_isWorkerRun = false;
			continue;
		}

		if (NULL == lpOverlapped)
		{
			continue;
		}

		auto overlappedEx = reinterpret_cast<stOverlappedEx*>(lpOverlapped);
		
		// Ŭ���̾�Ʈ�� ������ ������ ��
		if (FALSE == bSuccess ||
			(0 == dwIoSize && EnumOperation::ACCEPT != overlappedEx->_operation))
		{
			CloseSocket(overlappedEx->_sessionIndex, true);
			continue;
		}
		
		auto clientInfo = GetClientInfoRef(overlappedEx->_sessionIndex);

		switch (overlappedEx->_operation)
		{
		case EnumOperation::ACCEPT:
			if (clientInfo->AcceptCompletion() == true) 
			{
				++_clientCount;
				OnConnect(clientInfo->GetIndex());
			}
			else 
			{
				CloseSocket(clientInfo->GetIndex(), true);
			}
			break;

		case EnumOperation::SEND:
			clientInfo->SendCompletion(dwIoSize);
			break;

		case EnumOperation::RECV:
			OnReceive(clientInfo->GetIndex(), dwIoSize, clientInfo->RecvBuffer());

			clientInfo->BindRecv();
			break;

		default:
			printf("Client Index(%d)���� ���ܻ�Ȳ\n", clientInfo->GetIndex());
			break;
		}
	}
}

void IOCPserver::AcceptThread()
{
	while (_isAcceptRun)
	{
		auto curTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		for (auto client : _clientInfos)
		{
			if (client->IsConnectd())
			{
				continue;
			}

			if ((UINT64)curTimeSec < client->GetLatestClosedTimeSec())
			{
				continue;
			}

			auto diff = curTimeSec - client->GetLatestClosedTimeSec();
			if (diff <= RE_USE_SESSION_WAIT_TIMESEC)
			{
				continue;
			}

			client->PostAccept(_listenSocket, curTimeSec);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(32));
	}
}

