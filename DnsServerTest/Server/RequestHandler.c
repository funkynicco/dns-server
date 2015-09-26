#include "StdAfx.h"
#include "Server.h"

DWORD WINAPI RequestHandlerThread(LPVOID);

BOOL RequestHandlerInitialize(LPSERVER_INFO lpServerInfo, DWORD dwNumberOfThreads)
{
	RequestHandlerShutdown(lpServerInfo);

	if (dwNumberOfThreads > MAX_THREADS)
		return FALSE;

	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	lpRequestHandler->dwNumberOfThreads = 0;

	for (DWORD i = 0; i < dwNumberOfThreads; ++i)
	{
		lpRequestHandler->hThreads[i] = CreateThread(NULL, 0, RequestHandlerThread, lpServerInfo, CREATE_SUSPENDED, NULL);
		if (!lpRequestHandler->hThreads[i])
		{
			Error(__FUNCTION__ " - Could not create thread (%d of %d): %d - %s",
				i + 1,
				dwNumberOfThreads,
				GetLastError(),
				GetErrorMessage(GetLastError()));
			RequestHandlerShutdown(lpServerInfo);
			return FALSE;
		}
		++lpRequestHandler->dwNumberOfThreads;
	}

	lpRequestHandler->hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwNumberOfThreads);
	if (!lpRequestHandler->hIocp)
	{
		Error(__FUNCTION__ " - Could not create IO completion port: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
		RequestHandlerShutdown(lpServerInfo);
		return FALSE;
	}

	for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
		ResumeThread(lpRequestHandler->hThreads[i]);

	return TRUE;
}

void RequestHandlerShutdown(LPSERVER_INFO lpServerInfo)
{
	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	if (lpRequestHandler->hIocp)
	{
		for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(lpRequestHandler->hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(lpRequestHandler->hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(lpRequestHandler->hThreads[i], 0);
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)lpRequestHandler->hThreads[i]);
		}

		CloseHandle(lpRequestHandler->hThreads[i]);
	}
	lpRequestHandler->dwNumberOfThreads = 0;

	if (lpRequestHandler->hIocp)
		CloseHandle(lpRequestHandler->hIocp);
	lpRequestHandler->hIocp = NULL;
}

void RequestHandlerPostRequest(LPREQUEST_INFO lpRequestInfo)
{
	if (!PostQueuedCompletionStatus(lpRequestInfo->lpServerInfo->RequestHandler.hIocp, 1, 1, &lpRequestInfo->Overlapped))
	{
		Error(__FUNCTION__ " - PostQueuedCompletionStatus returned FALSE, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DestroyRequestInfo(lpRequestInfo);
	}
}

void RequestHandlerProcessRequest(LPREQUEST_INFO lpRequestInfo)
{
	// kek wak po le banana
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
	LoggerWrite(__FUNCTION__ " - Request from %s:%d", addrtext, ntohs(lpRequestInfo->SocketAddress.sin_port));

	strcpy(lpRequestInfo->Buffer, "hello");
	lpRequestInfo->dwLength = 5;
	PostSend(lpRequestInfo, IO_SEND);
	return;

	if (lpRequestInfo->dwLength < sizeof(DNS_HEADER))
	{
		Error(__FUNCTION__ " - Received data is too small for a DNS_HEADER frame (%u bytes)", lpRequestInfo->dwLength);
		DestroyRequestInfo(lpRequestInfo);
		return;
	}

	DNS_HEADER DnsHeader;
	memcpy(&DnsHeader, lpRequestInfo->Buffer, sizeof(DNS_HEADER));
	FlipDnsHeader(&DnsHeader);

#if 0
	if (DnsHeader.NumberOfAnswers ||
		DnsHeader.NumberOfAuthorities ||
		DnsHeader.NumberOfAdditional)
	{
		Error(__FUNCTION__ " - Answers/Authorities/Additional was not zero, unsupported!!");
		DestroyRequestInfo(lpRequestInfo);
		return;
	}
#endif

#if 1
	char aLabels[128][64];
	char domainName[1024];
	DWORD dwNumLabels = 0;

	const char* ptr = lpRequestInfo->Buffer + sizeof(DNS_HEADER);
	const char* end = lpRequestInfo->Buffer + lpRequestInfo->dwLength;

#define ASSERT_LEN(__l) if ((ptr + (__l)) > end) { Error(__FUNCTION__ " - Failed to read %u bytes in buffer", (__l)); DestroyRequestInfo(lpRequestInfo); return; }
#define DIE() { DestroyRequestInfo(lpRequestInfo); return; }

	for (u_short i = 0; i < DnsHeader.NumberOfQuestions; ++i)
	{
		dwNumLabels = 0;
		ASSERT_LEN(1);
		BYTE labelLen;
		while (ptr < end && (labelLen = *ptr++))
		{
			if (labelLen >= sizeof(aLabels[0]))
			{
				Error(__FUNCTION__ " - Too big label (%u bytes)", labelLen);
				DIE();
			}

			if (dwNumLabels >= 128)
			{
				Error(__FUNCTION__ " - Too many labels (128)");
				DIE();
			}

			ASSERT_LEN(labelLen);
			memcpy(aLabels[dwNumLabels], ptr, labelLen); ptr += labelLen;
			aLabels[dwNumLabels++][labelLen] = 0;
		}
		ASSERT_LEN(4);
		u_short questionType = *((u_short*)ptr); ptr += 2;
		u_short questionClass = *((u_short*)ptr); ptr += 2;

		AssembleDomainFromLabels(domainName, aLabels, dwNumLabels);
		Error(__FUNCTION__ " - Resolve domain name: '%s'", domainName);
	}

	ResolveDns(lpRequestInfo);
#endif
	/*strcpy(lpRequestInfo->Buffer, "w3w lel kaka");
	lpRequestInfo->dwLength = strlen(lpRequestInfo->Buffer);
	PostSend(lpRequestInfo);*/
}

DWORD WINAPI RequestHandlerThread(LPVOID lp)
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)lp;
	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	LPREQUEST_INFO lpRequestInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(lpRequestHandler->hIocp, &dwBytesTransferred, &ulCompletionKey, (LPOVERLAPPED*)&lpRequestInfo, INFINITE))
		{
			Error(__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
			Sleep(50);
			continue;
		}

		if (dwBytesTransferred == 0) // close thread request
			break;

		RequestHandlerProcessRequest(lpRequestInfo);
	}

	return 0;
}