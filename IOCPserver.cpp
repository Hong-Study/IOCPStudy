#include "pch.h"
#include "IOCPserver.h"

bool IOCPserver::Init(const UINT32 maxThreadCount)
{
	WSADATA wsaData;

	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != nRet)
	{
		printf("[에러] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	//연결지향형 TCP , Overlapped I/O 소켓을 생성
	_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == _listenSocket)
	{
		printf("[에러] socket()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	_maxWorkerThreadCount = maxThreadCount;

	printf("소켓 초기화 성공\n");
	return true;
}

bool IOCPserver::BindandListen(short bindPort, int acceptIp)
{
	SOCKADDR_IN		stServerAddr;
	stServerAddr.sin_family = AF_INET;
	stServerAddr.sin_port = htons(bindPort); //서버 포트를 설정한다.		
	//어떤 주소에서 들어오는 접속이라도 받아들이겠다.
	//보통 서버라면 이렇게 설정한다. 만약 한 아이피에서만 접속을 받고 싶다면
	//그 주소를 inet_addr함수를 이용해 넣으면 된다.
	stServerAddr.sin_addr.s_addr = htonl(acceptIp);

	//위에서 지정한 서버 주소 정보와 cIOCompletionPort 소켓을 연결한다.
	int nRet = bind(_listenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
	if (0 != nRet)
	{
		printf("[에러] bind()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	//접속 요청을 받아들이기 위해 cIOCompletionPort소켓을 등록하고 
	//접속대기큐를 5개로 설정 한다.
	nRet = listen(_listenSocket, 5);
	if (0 != nRet)
	{
		printf("[에러] listen()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	//CompletionPort객체 생성 요청을 한다.
	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, _maxWorkerThreadCount);
	if (NULL == _iocpHandle)
	{
		printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
		return false;
	}

	auto hIOCPHandle = CreateIoCompletionPort((HANDLE)_listenSocket, _iocpHandle, (UINT32)0, 0);
	if (nullptr == hIOCPHandle)
	{
		printf("[에러] listen socket IOCP bind 실패 : %d\n", WSAGetLastError());
		return false;
	}

	printf("서버 등록 성공..\n");
	return true;
}

bool IOCPserver::StartServer(const UINT32 maxClientCount)
{
	CreateClient(maxClientCount);

	//접속된 클라이언트 주소 정보를 저장할 구조체
	bool bRet = CreateWorkerThread();
	if (false == bRet) {
		return false;
	}

	bRet = CreateAcceptThread();
	if (false == bRet) {
		return false;
	}

	printf("서버 시작\n");
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

	printf("WokerThread 시작..\n");
	return true;
}

bool IOCPserver::CreateAcceptThread()
{
	_acceptThread = std::thread([this]() { AcceptThread(); });

	printf("AccepterThread 시작..\n");
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
	//함수 호출 성공 여부
	BOOL bSuccess = TRUE;
	//Overlapped I/O작업에서 전송된 데이터 크기
	DWORD dwIoSize = 0;
	//I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
	LPOVERLAPPED lpOverlapped = NULL;

	while (_isWorkerRun)
	{
		bSuccess = GetQueuedCompletionStatus(
			_iocpHandle,
			&dwIoSize,
			(PULONG_PTR)&key,
			(LPOVERLAPPED*)&lpOverlapped,
			INFINITE);

		// 사용자 쓰레드 종료 메시지 처리
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
		
		// 클라이언트가 연결을 끊었을 때
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
			printf("Client Index(%d)에서 예외상황\n", clientInfo->GetIndex());
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

