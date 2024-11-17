#pragma once

class ClientInfo
{
public:
	ClientInfo();
	void Init(const UINT32 index, HANDLE iocpHandle);

	UINT32 GetIndex() { return _index; }
	SOCKET GetSock() { return _socket; }
	UINT64 GetLatestClosedTimeSec() { return _latestClosedTimeSec; }
	bool IsConnectd() { return _isConnect == 1; }
	char* RecvBuffer() { return _recvBuf.data(); }

	bool OnConnect(HANDLE iocpHandle, SOCKET socket);

	bool BindIOCompletionPort(HANDLE iocpHandle);

	void Close(bool isForce = false);
	void Clear() { }

	bool PostAccept(SOCKET listenSock, const UINT64 curTimeSec);
	bool SendMsg(const UINT32 size, char* data);

	bool AcceptCompletion();
	void SendCompletion(const UINT32 dataSize);

	bool BindRecv();
	bool BindSend();

private:
	INT32			_index = 0;
	HANDLE			_iocpHandle = INVALID_HANDLE_VALUE;

	INT64			_isConnect = 0;
	UINT64			_latestClosedTimeSec = 0;

	SOCKET			_socket;			//Cliet와 연결되는 소켓

	stOverlappedEx	_acceptContext;
	std::array<char, 64>			_acceptBuf;

	stOverlappedEx					_recvOverlappedEx;	//RECV Overlapped I/O작업을 위한 변수	
	std::array<char, MAX_SOCKBUF>	_recvBuf; //데이터 버퍼

	std::mutex _sendLock;
	std::queue<stOverlappedEx*> _sendQueue;

};