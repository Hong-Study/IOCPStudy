#pragma once
#include "pch.h"

class IOCPserver
{
public:
	IOCPserver(void) {}

	~IOCPserver(void)
	{
		//������ ����� ������.
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

	// IOCP Port���� �߻��� �̺�Ʈ ó���� ������
	bool CreateWorkerThread();
	bool CreateAcceptThread();

	void CloseSocket(UINT32 clientIndex, bool bIsForce = false);

private:
	void WorkerThread();
	void AcceptThread();

private:
	UINT32 _maxWorkerThreadCount = 0;

	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<ClientInfoRef> _clientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET		_listenSocket = INVALID_SOCKET;

	//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	int			_clientCount = 0;

	//IO Worker ������
	std::vector<std::thread> _workerThreads;

	//Accept ������
	std::thread	_acceptThread;

	//CompletionPort��ü �ڵ�
	HANDLE		_iocpHandle = INVALID_HANDLE_VALUE;

	//�۾� ������ ���� �÷���
	bool		_isWorkerRun = true;

	//���� ������ ���� �÷���
	bool		_isAcceptRun = true;
};

