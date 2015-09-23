#include "StdAfx.h"
#include "Server.h"

BOOL StartServer(LPSERVER_INFO lpServerInfo, int port)
{
	if (lpServerInfo->Socket != INVALID_SOCKET)
		return FALSE;

	lpServerInfo->Socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (lpServerInfo->Socket == INVALID_SOCKET)
		return FALSE;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(lpServerInfo->Socket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		DWORD dwError = WSAGetLastError();
		SAFE_CLOSE_SOCKET(lpServerInfo->Socket);
		WSASetLastError(dwError);
		return FALSE;
	}

	if (CreateIoCompletionPort((HANDLE)lpServerInfo->Socket, lpServerInfo->hNetworkIocp, lpServerInfo->Socket, 0) != lpServerInfo->hNetworkIocp)
	{
		printf(__FUNCTION__ " - Failed to associate socket to network IOCP, code: %u\n", GetLastError());
		DWORD dwError = GetLastError();
		SAFE_CLOSE_SOCKET(lpServerInfo->Socket);
		SetLastError(dwError);
		return FALSE;
	}

	LPREQUEST_INFO lpRequestInfo = AllocateRequestInfo(lpServerInfo);

	LPNETWORK_BUFFER lpNetworkBuffer = AllocateNetworkBuffer(lpRequestInfo);

	ServerPostReceive(lpNetworkBuffer);

	return TRUE;
}

void StopServer(LPSERVER_INFO lpServerInfo)
{
	/*while (lpServerInfo->lpPendingRequests)
	{
		LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpPendingRequests;
		lpServerInfo->lpPendingRequests = lpServerInfo->lpPendingRequests->next;
		DestroyRequestInfo(lpRequestInfo);
	}*/

	SAFE_CLOSE_SOCKET(lpServerInfo->Socket);
}

void ProcessServer(LPSERVER_INFO lpServerInfo)
{
#if 0
	SOCKADDR_IN addr;
	int addrlen;
	char buffer[65536];
	struct timeval tv;
	struct fd_set fd;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&fd);
	FD_SET(lpServerInfo->Socket, &fd);
	if (select(1, &fd, NULL, NULL, &tv) <= 0)
		return;

	addrlen = sizeof(addr);
	int len = recvfrom(lpServerInfo->Socket, buffer, sizeof(buffer), 0, (LPSOCKADDR)&addr, &addrlen);
	char addrtext[64];
	GetIPFromSocketAddress(&addr, addrtext, sizeof(addrtext));
	printf("Received %d bytes from %s:%d\n", len, addrtext, ntohs(addr.sin_port));

	if (len < sizeof(DNS_HEADER))
	{
		printf("Len(%d) is less than DNS_HEADER(%d)\n", len, sizeof(DNS_HEADER));
		return;
	}

	LPDNS_HEADER lpDnsHeader = (LPDNS_HEADER)&buffer[0];
	FlipDnsHeader(lpDnsHeader);

	LPREQUEST_INFO lpRequestInfo = AllocateRequestInfo(lpServerInfo);
	memcpy(&lpRequestInfo->DnsHeader, lpDnsHeader, sizeof(DNS_HEADER));
	memcpy(lpRequestInfo->HeaderData, buffer + sizeof(DNS_HEADER), len - sizeof(DNS_HEADER));
	memcpy(&lpRequestInfo->SocketAddress, &addr, sizeof(SOCKADDR_IN));

	RequestHandlerPostRequest(lpRequestInfo);

	const unsigned char* pReadPtr = buffer + sizeof(DNS_HEADER);
	const unsigned char* pEndPtr = buffer + len;
	const unsigned char* pFind;

	printf(
		"Transaction ID: %d\n"
		"Flags: %04x\n"
		"NumberOfQuestions: %d\n"
		"NumberOfAnswers: %d\n"
		"NumberOfAuthorities: %d\n"
		"NumberOfAdditional: %d\n",
		lpDnsHeader->TransactionID,
		lpDnsHeader->Flags,
		lpDnsHeader->NumberOfQuestions,
		lpDnsHeader->NumberOfAnswers,
		lpDnsHeader->NumberOfAuthorities,
		lpDnsHeader->NumberOfAdditional);

#define ASSERT_READ(__n) if(pReadPtr+__n>pEndPtr){printf("ERR_OVER_READ\n");return;}
#define FIND_NULL() while(*(pFind=pReadPtr)){if(pReadPtr++>=pEndPtr){printf("ERROR_FIND_NULL\n");return;}}

	for (u_short i = 0; i < lpDnsHeader->NumberOfQuestions; ++i)
	{
		const unsigned char* begin = pReadPtr;
		FIND_NULL();
		char nameField[256];
		memcpy(nameField, begin, pFind - begin);
		nameField[pFind - begin] = 0;
		for (size_t j = 0; j < (size_t)(pFind - begin); ++j)
		{
			if (nameField[j] < 30)
				nameField[j] = '.';
		}
		printf("[%d] %s\n", i, nameField);
		pReadPtr++;

		ASSERT_READ(2);
		u_short questionType = *((u_short*)pReadPtr); pReadPtr += 2;
		printf("\tQuestion Type: %d\n", ntohs(questionType));

		ASSERT_READ(2);
		u_short questionClass = *((u_short*)pReadPtr); pReadPtr += 2;
		printf("\tQuestion Class: %d\n", ntohs(questionClass));
}
#endif
}