#include "pch.h"
#include "EchoServer.h"

void EchoServer::OnConnect(const UINT32 clientIndex)
{
	printf("[OnConnect] 클라이언트: Index(%d)\n", clientIndex);
}

void EchoServer::OnClose(const UINT32 clientIndex)
{
	printf("[OnClose] 클라이언트: Index(%d)\n", clientIndex);
}

void EchoServer::OnReceive(const UINT32 clientIndex, const UINT32 size, char* data)
{
	printf("[OnReceive] 클라이언트: Index(%d), dataSize(%d)\n", clientIndex, size);

	PacketData packet;
	packet.Set(clientIndex, size, data);

	std::lock_guard<std::mutex> guard(_lock);
	_packetDataQueue.push_back(packet);
}

void EchoServer::Run(const UINT32 maxClient)
{
	_isRunProcessThread = true;
	_processthread = std::thread([this]() { ProcessPacket(); });

	StartServer(maxClient);
}

void EchoServer::End()
{
	_isRunProcessThread = false;

	if (_processthread.joinable())
	{
		_processthread.join();
	}

	StopServer();
}

void EchoServer::ProcessPacket()
{
	while (_isRunProcessThread)
	{
		auto packetData = DequqPacketData();
		if (packetData.DataSize != 0)
		{
			SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.Data);
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

PacketData EchoServer::DequqPacketData()
{
	PacketData packetData;

	std::lock_guard<std::mutex> guard(_lock);
	if (_packetDataQueue.empty())
	{
		return PacketData();
	}

	packetData.Set(_packetDataQueue.front());

	_packetDataQueue.front().Release();
	_packetDataQueue.pop_front();

	return packetData;
}
