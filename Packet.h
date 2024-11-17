#pragma once

struct PacketData
{
public:
	UINT32 SessionIndex = 0;
	UINT32 DataSize = 0;
	char* Data = nullptr;

	void Set(PacketData& vlaue)
	{
		SessionIndex = vlaue.SessionIndex;
		DataSize = vlaue.DataSize;

		Data = new char[vlaue.DataSize];
		CopyMemory(Data, vlaue.Data, vlaue.DataSize);
	}

	void Set(UINT32 sessionIndex_, UINT32 dataSize_, char* pData)
	{
		SessionIndex = sessionIndex_;
		DataSize = dataSize_;

		Data = new char[dataSize_];
		CopyMemory(Data, pData, dataSize_);
	}

	void Release()
	{
		delete Data;
	}
};