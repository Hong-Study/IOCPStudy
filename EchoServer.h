#pragma once
#include "IOCPserver.h"
#include "Packet.h"

class EchoServer : public IOCPserver
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;

	virtual void OnConnect(const UINT32 clientIndex) override;
	virtual void OnClose(const UINT32 clientIndex) override;
	virtual void OnReceive(const UINT32 clientIndex, const UINT32 size, char* data) override;

	void Run(const UINT32 maxClient);
	void End();

private:
	void ProcessPacket();
	PacketData DequqPacketData();

private:
	bool _isRunProcessThread = false;

	std::thread _processthread;

	std::mutex _lock;
	std::deque<PacketData> _packetDataQueue;
};


