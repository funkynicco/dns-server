#include "StdAfx.h"

void ReadResponse(const char* buffer, int len)
{
	// N/A
}

void PostRequest(SOCKET Socket, LPSOCKADDR_IN lpServerAddr, const char* domain)
{
	printf("Sending request to DNS server...\n");

	DNS_HEADER header;
	ZeroMemory(&header, sizeof(DNS_HEADER));

	header.TransactionID = htons(1337);
	header.NumberOfQuestions = htons(1);

	char buffer[1024];
	char* ptr = buffer;

	memcpy(ptr, &header, sizeof(DNS_HEADER)); ptr += sizeof(DNS_HEADER);

	ptr += sprintf(ptr, "\x04test\x03hue\x03com");
	*((u_short*)ptr) = 0; ptr += 2; // question type
	*((u_short*)ptr) = 0; ptr += 2; // question class

	int sent = sendto(Socket, buffer, ptr - buffer, 0, (LPSOCKADDR)lpServerAddr, sizeof(SOCKADDR_IN));
	if (sent <= 0)
	{
		printf("Failed to send request, code: %u\n", WSAGetLastError());
		return;
	}

	printf("Successfully sent %d bytes, waiting for request ...\n", sent);

	// wait for request
	struct fd_set fd;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1;

	time_t tmTimeout = time(NULL) + 5;

	while (1)
	{
		time_t tmNow = time(NULL);
		if (tmNow >= tmTimeout)
		{
			printf("No response from DNS server after 5 seconds\n");
			break;
		}

		FD_ZERO(&fd);
		FD_SET(Socket, &fd);
		if (select(1, &fd, NULL, NULL, &tv) > 0)
		{
			int addrlen = sizeof(SOCKADDR_IN);
			int len = recvfrom(Socket, buffer, sizeof(buffer), 0, (LPSOCKADDR)lpServerAddr, &addrlen);
			if (len > 0)
			{
				printf("Received %d bytes from DNS server\n", len);
				ReadResponse(buffer, len);
			}
			else
				printf("recvfrom error, code: %u\n", WSAGetLastError());

			break;
		}
	}
}

int main(int argc, char* argv[])
{
	SetConsoleTitle(L"DNS Service Tester");

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ServerAddr.sin_port = htons(53);

	SOCKET Socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (Socket != INVALID_SOCKET)
	{
		BOOL bShutdown = FALSE;
		while (!bShutdown)
		{
			if (_kbhit())
			{
				int ch = _getch();
				if (ch == 0)
					ch = _getch();

				switch (ch)
				{
				case VK_ESCAPE:
					bShutdown = TRUE;
					break;
				case VK_SPACE:
					PostRequest(Socket, &ServerAddr, "google.com");
					break;
				}
			}

			Sleep(50);
		}

		closesocket(Socket);
	}
	else
		printf("Failed to create socket: %u\n", WSAGetLastError());

	WSACleanup();
	return 0;
}