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
	header.Flags = htons(0x0100);

	char buffer[1024];
	char* ptr = buffer;

	memcpy(ptr, &header, sizeof(DNS_HEADER)); ptr += sizeof(DNS_HEADER);

#define ADD_LABEL(__ptr, __label) { size_t __label_len = strlen(__label); *ptr++ = (BYTE)__label_len; memcpy(__ptr, __label, __label_len); __ptr += __label_len; }

	// test.hue.com
	//ADD_LABEL(ptr, "test");
	ADD_LABEL(ptr, "microsoft");
	ADD_LABEL(ptr, "com");

	*ptr++ = 0; // must end the labels with \0
	*((u_short*)ptr) = htons(1); ptr += 2; // question type
	*((u_short*)ptr) = htons(1); ptr += 2; // question class

	int sent = sendto(Socket, buffer, ptr - buffer, 0, (LPSOCKADDR)lpServerAddr, sizeof(SOCKADDR_IN));
	if (sent <= 0)
	{
		printf("Failed to send request, code: %u\n", WSAGetLastError());
		return;
	}

	if (sent < (ptr - buffer))
	{
		printf("Failed to send all %u bytes!\n", (ptr - buffer));
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
				for (int i = 0; i < len; ++i)
				{
					if (i > 0)
					{
						if (i % 16 == 0)
							printf("\n");
						else
							printf(" ");
					}

					printf("%02x", (unsigned char)buffer[i]);
				}
				printf("\n");
				ReadResponse(buffer, len);
			}
			else
				printf("recvfrom error, code: %u\n", WSAGetLastError());

			break;
		}
	}
}

void PrintOperationTime(double value)
{
	int n = 0;
	while (value < 1.0)
	{
		value *= 1000.0;
		++n;
	}

	switch (n)
	{
	case 0: printf("Operation took %.3f seconds\n", value); break;
	case 1: printf("Operation took %.3f milliseconds\n", value); break;
	case 2: printf("Operation took %.3f microseconds\n", value); break;
	case 3: printf("Operation took %.3f nanoseconds\n", value); break;
	default:
		printf(__FUNCTION__ " - ERROR, n: %d\n", n);
		break;
	}
}

int main(int argc, char* argv[])
{
	SetConsoleTitle(L"DNS Service Tester");

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //inet_addr("8.8.8.8");
	ServerAddr.sin_port = htons(53);

	IocpDnsTest iocp;

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
				{
					vector<string> result;
					double responseTime;
					printf(__FUNCTION__ " - Result: %s\n", iocp.ResolveDomain("google.com", result, &responseTime) ? "Succeeded" : "Failed");
					PrintOperationTime(responseTime);
					for (vector<string>::iterator it = result.begin(); it != result.end(); ++it)
						printf("==> %s\n", it->c_str());
				}
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