#pragma once
#include "pch.h"

class IOCPserver
{
public:
	IOCPserver(void) {}

	~IOCPserver(void)
	{
		//윈속의 사용을 끝낸다.
		WSACleanup();
	}

public:
	bool Init(const UINT32 maxThreadCount);
	bool BindandListen(short bindPort, int acceptIp = INADDR_ANY);
	bool StartServer(const UINT32 maxClientCount);
	void StopServer();

	bool SendMsg(UINT32 clientIndex, UINT32 size, char* data);

protected:
	virtual void OnConnect(const UINT32 clientIndex) = 0;
	virtual void OnClose(const UINT32 clientIndex) = 0;
	virtual void OnReceive(const UINT32 clientIndex, const UINT32 size, char* data) = 0;

private:
	void CreateClient(const UINT32 maxClientCount);

	ClientInfoRef GetEmptyClientInfoRef();
	ClientInfoRef GetClientInfoRef(const UINT32 index);

	// IOCP Port에서 발생한 이벤트 처리용 스레드
	bool CreateWorkerThread();
	bool CreateAcceptThread();

	void CloseSocket(UINT32 clientIndex, bool bIsForce = false);

private:
	void WorkerThread();
	void AcceptThread();

private:
	UINT32 _maxWorkerThreadCount = 0;

	//클라이언트 정보 저장 구조체
	std::vector<ClientInfoRef> _clientInfos;

	//클라이언트의 접속을 받기위한 리슨 소켓
	SOCKET		_listenSocket = INVALID_SOCKET;

	//접속 되어있는 클라이언트 수
	int			_clientCount = 0;

	//IO Worker 스레드
	std::vector<std::thread> _workerThreads;

	//Accept 스레드
	std::thread	_acceptThread;

	//CompletionPort객체 핸들
	HANDLE		_iocpHandle = INVALID_HANDLE_VALUE;

	//작업 쓰레드 동작 플래그
	bool		_isWorkerRun = true;

	//접속 쓰레드 동작 플래그
	bool		_isAcceptRun = true;
};

