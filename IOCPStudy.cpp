#include "pch.h"
#include "EchoServer.h"

#include <iostream>
#include <string>

int main()
{
	EchoServer server;
	
	// 소켓 초기화
	server.Init(4);

	// 서버 주소와 연결하고 등록 시킨다.
	server.BindandListen(9000);

	// 시작
	server.Run(3);

	printf("아무 키나 누를 때까지 대기합니다\n");
	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	return 0;
}