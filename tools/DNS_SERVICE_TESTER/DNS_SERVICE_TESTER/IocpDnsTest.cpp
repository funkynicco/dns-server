#include "StdAfx.h"
#include "IocpDnsTest.h"

static DWORD WINAPI IocpThread(LPVOID lp)
{
	return static_cast<IocpDnsTest*>(lp)->WorkerThread();
}

IocpDnsTest::IocpDnsTest() :
	m_socket(INVALID_SOCKET),
	m_hThread(NULL),
	m_hIocp(NULL),
	m_dwRequestResponseBucketIndexPoolCount(0),
	m_lpFreeRequests(NULL),
	m_dwAllocatedRequests(0)
{
	InitializeCriticalSectionAndSpinCount(&m_cs, 2000);
	InitializeCriticalSectionAndSpinCount(&m_csAlloc, 2000);
	QueryPerformanceFrequency(&m_liFrequency);

	for (int i = 0; i < ARRAYSIZE(m_requestResponseBucketIndexPool); ++i)
	{
		m_requestResponseBucketIndexPool[m_dwRequestResponseBucketIndexPoolCount++] = i;
	}

	m_socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == INVALID_SOCKET)
		return;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(53);

	if (bind(m_socket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		SAFE_CLOSE_SOCKET(m_socket);
		printf(__FUNCTION__ " - Bind failed, code: %u\n", WSAGetLastError());
		return;
	}

	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!m_hIocp)
	{
		SAFE_CLOSE_SOCKET(m_socket);
		return;
	}

	CreateIoCompletionPort((HANDLE)m_socket, m_hIocp, 0, 0);

	m_hThread = CreateThread(NULL, 0, IocpThread, this, CREATE_SUSPENDED, NULL);
	if (!m_hThread)
	{
		CloseHandle(m_hIocp);
		SAFE_CLOSE_SOCKET(m_socket);
		return;
	}

	ResumeThread(m_hThread);

	LPREQUEST_INFO lpRequestInfo = AllocateRequestInfo(m_socket, NULL);
	lpRequestInfo->SocketAddress.sin_family = AF_INET;
	lpRequestInfo->SocketAddress.sin_addr.s_addr = inet_addr("0.0.0.0");
	lpRequestInfo->SocketAddress.sin_port = htons(53);
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);

	if (!PostReceive(lpRequestInfo, IO_RECV))
		printf(__FUNCTION__ " - PostReceive failed\n");
}

IocpDnsTest::~IocpDnsTest()
{
	if (m_hThread != NULL)
	{
		if (m_hIocp)
		{
			PostQueuedCompletionStatus(m_hIocp, 0, 0, NULL);
			WaitForSingleObject(m_hThread, INFINITE);
		}
		else
			TerminateThread(m_hThread, 0);
	}

	while (m_lpFreeRequests)
	{
		LPREQUEST_INFO lpRequestInfo = m_lpFreeRequests;
		m_lpFreeRequests = m_lpFreeRequests->next;
		free(GetRealMemoryPointer(lpRequestInfo));
	}

	SAFE_CLOSE_SOCKET(m_socket);
	DeleteCriticalSection(&m_csAlloc);
	DeleteCriticalSection(&m_cs);
}

BOOL IocpDnsTest::ResolveDomain(const char* domain, vector<string>& result, double* pdResponseTime, DWORD dwMaxWaitTime)
{
	CriticalSection lock(&m_cs, TRUE);

	if (m_dwRequestResponseBucketIndexPoolCount == 0)
		return FALSE;

	LPREQUEST_INFO lpRequestInfo = AllocateRequestInfo(m_socket, NULL);

	// alloc an entry in the request response pool for the transaction id
	u_short index = m_requestResponseBucketIndexPool[--m_dwRequestResponseBucketIndexPoolCount];
	lock.Leave();

	LPDNS_HEADER lpDnsHeader = (LPDNS_HEADER)lpRequestInfo->Buffer;
	lpDnsHeader->TransactionID = htons(index); // set the bucket index as transaction id
	lpDnsHeader->Flags = htons(1 << 9); // RD=1
	lpDnsHeader->NumberOfQuestions = htons(1);

	char* ptr = lpRequestInfo->Buffer + sizeof(DNS_HEADER);
	*ptr++ = 5;
	memcpy(ptr, "nprog", 5); ptr += 5;
	*ptr++ = 3;
	memcpy(ptr, "com", 3); ptr += 3;
	*ptr++ = 0;

	*(u_short*)ptr = htons(1); ptr += 2; // type
	*(u_short*)ptr = htons(1); ptr += 2; // class

	lpRequestInfo->dwLength = (DWORD)(ptr - lpRequestInfo->Buffer);

	lpRequestInfo->SocketAddress.sin_family = AF_INET;
	lpRequestInfo->SocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	lpRequestInfo->SocketAddress.sin_port = htons(53);

	lock.Enter(); // lock is auto freed on lock's destructor
	m_lpRequestResponseBuckets[index] = lpRequestInfo; // send info
	lock.Leave();

	HANDLE hWaitObject = lpRequestInfo->hWaitObject;

	if (!PostSend(lpRequestInfo, IO_SEND))
	{
		FreeRequestInfo(lpRequestInfo);
		return FALSE;
	}

	LARGE_INTEGER liStart, liEnd;
	QueryPerformanceCounter(&liStart);

	if (WaitForSingleObject(hWaitObject, dwMaxWaitTime) == WAIT_TIMEOUT)
	{
		printf(__FUNCTION__ " - Wait timed out ...\n");
		CancelIoEx((HANDLE)lpRequestInfo->Socket, &lpRequestInfo->Overlapped);
		return FALSE;
	}

	lock.Enter();
	lpRequestInfo = m_lpRequestResponseBuckets[index]; // it was swapped out ...

	// lpRequestInfo is now the response (swapped out with the IO_RECV one ...)

	QueryPerformanceCounter(&liEnd);
	if (pdResponseTime)
		*pdResponseTime = double(liEnd.QuadPart - liStart.QuadPart) / m_liFrequency.QuadPart;

	//LPREQUEST_INFO lpRequestResponse = m_lpRequestResponseBuckets[index];
	m_requestResponseBucketIndexPool[m_dwRequestResponseBucketIndexPoolCount++] = index;

	// TODO
	printf(__FUNCTION__ " - [%p] Response received: %u bytes\n", lpRequestInfo, lpRequestInfo->dwLength);

	FreeRequestInfo(lpRequestInfo);
	return FALSE; // temp
}

LPREQUEST_INFO IocpDnsTest::AllocateRequestInfo(SOCKET Socket, LPDWORD lpdwAllocatedRequests)
{
	EnterCriticalSection(&m_csAlloc);
	LPREQUEST_INFO lpRequestInfo = m_lpFreeRequests;
	if (lpRequestInfo)
		m_lpFreeRequests = m_lpFreeRequests->next;
	DWORD dwAlloc = ++m_dwAllocatedRequests;
	if (lpdwAllocatedRequests)
		*lpdwAllocatedRequests = m_dwAllocatedRequests;
	LeaveCriticalSection(&m_csAlloc);

	if (!lpRequestInfo)
		lpRequestInfo = (LPREQUEST_INFO)((char*)malloc(sizeof(REQUEST_INFO) + 1) + 1);

	printf(__FUNCTION__ " - Alloc %p, Total: %u\n", lpRequestInfo, dwAlloc);

	ClearDestroyed(lpRequestInfo);

	ZeroMemory(lpRequestInfo, sizeof(REQUEST_INFO));

	lpRequestInfo->hWaitObject = CreateEvent(NULL, TRUE, FALSE, NULL); // only alloc this once ...?

	lpRequestInfo->Socket = Socket;
	return lpRequestInfo;
}

void IocpDnsTest::FreeRequestInfo(LPREQUEST_INFO lpRequestInfo)
{
	if (IsDestroyed(lpRequestInfo))
		printf(__FUNCTION__ " - [FATAL] Attempt to free already destroyed object %p\n", lpRequestInfo);

	CloseHandle(lpRequestInfo->hWaitObject); // ... think of something better

	ZeroMemory(lpRequestInfo, sizeof(REQUEST_INFO)); // temp
	SetDestroyed(lpRequestInfo);

	EnterCriticalSection(&m_csAlloc);
	lpRequestInfo->next = m_lpFreeRequests;
	m_lpFreeRequests = lpRequestInfo;
	DWORD dwAlloc = --m_dwAllocatedRequests;
	LeaveCriticalSection(&m_csAlloc);

	printf(__FUNCTION__ " - Free %p, Total: %u\n", lpRequestInfo, dwAlloc);

	//CloseHandle(lpRequestInfo->hWaitObject);
	//free(GetRealMemoryPointer(lpRequestInfo));
}

DWORD IocpDnsTest::WorkerThread()
{
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;
	LPREQUEST_INFO lpRequestInfo;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(
			m_hIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpRequestInfo,
			INFINITE))
		{
			DWORD dwError = GetLastError();

			switch (dwError)
			{
			case ERROR_OPERATION_ABORTED:
				printf(__FUNCTION__ " - Aborted %p\n", lpRequestInfo);
				if (lpRequestInfo)
					FreeRequestInfo(lpRequestInfo);
				break;
			default:
				printf(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %Id, lpRequestInfo: %p, Socket: %Id)\n",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					lpRequestInfo,
					lpRequestInfo ? lpRequestInfo->Socket : INVALID_SOCKET);
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpRequestInfo == NULL)
			break; // kill thread

		printf(__FUNCTION__ " - I/O complete %p, IOMode: %s\n", lpRequestInfo, lpRequestInfo->IOMode == IO_SEND ? "IO_SEND" : "IO_RECV");
		m_dataLogger.Write(lpRequestInfo->IOMode, lpRequestInfo->Buffer, dwBytesTransferred);

		switch (lpRequestInfo->IOMode)
		{
		case IO_RECV:
			lpRequestInfo->dwLength = dwBytesTransferred;
			ProcessResponse(lpRequestInfo);
			break;
		case IO_SEND:
			//FreeRequestInfo(lpRequestInfo);// do not free this cause we depend on it
			break;
		}
	}

	return 0;
}

void IocpDnsTest::ProcessResponse(LPREQUEST_INFO lpRequestInfo)
{
	CriticalSection lock(&m_cs, TRUE);

	LPDNS_HEADER lpDnsHeader = (LPDNS_HEADER)lpRequestInfo->Buffer; // in network byte order ...
	int index = ntohs(lpDnsHeader->TransactionID);

	if (index < 0 ||
		index > ARRAYSIZE(m_lpRequestResponseBuckets) ||
		!m_lpRequestResponseBuckets[index])
	{
		printf(__FUNCTION__ " - Invalid transaction ID %u in response from DNS server.\n", index);
		if (!PostReceive(lpRequestInfo, IO_RECV))
		{
			FreeRequestInfo(lpRequestInfo);
			printf(__FUNCTION__ " - PostReceive failed\n");
		}
		return;
	}

	HANDLE hWaitObject = m_lpRequestResponseBuckets[index]->hWaitObject;

	LPREQUEST_INFO lpTemp = m_lpRequestResponseBuckets[index];
	m_lpRequestResponseBuckets[index] = lpRequestInfo;
	lpRequestInfo = lpTemp;
	SetEvent(hWaitObject);

	if (!PostReceive(lpRequestInfo, IO_RECV))
	{
		printf(__FUNCTION__ " - PostReceive failed of %p\n", lpRequestInfo);
		FreeRequestInfo(lpRequestInfo);
	}
}

BOOL IocpDnsTest::PostReceive(LPREQUEST_INFO lpRequestInfo, int IOMode)
{
	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = sizeof(lpRequestInfo->Buffer);

	lpRequestInfo->dwFlags = 0;
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
	lpRequestInfo->IOMode = IOMode;

	// add to pending WSARecvFrom

	int err = WSARecvFrom(
		lpRequestInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		&lpRequestInfo->dwFlags,
		(LPSOCKADDR)&lpRequestInfo->SocketAddress,
		&lpRequestInfo->SockAddrLen,
		&lpRequestInfo->Overlapped,
		NULL);

	printf(__FUNCTION__ " - lpRequestInfo %p\n", lpRequestInfo);

	if (err == 0) // completed directly
		return TRUE;

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING) // posted WSARecvFrom
		return TRUE;

	/*EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
	Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);*/

	printf(__FUNCTION__ " - [ERROR] WSARecvFrom returned %d, code: %u\n", err, dwError);

	return FALSE;
}

BOOL IocpDnsTest::PostSend(LPREQUEST_INFO lpRequestInfo, int IOMode)
{
	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = lpRequestInfo->dwLength;

	lpRequestInfo->IOMode = IOMode;

	// add to pending wsasendto

	int err = WSASendTo(
		lpRequestInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		0,
		(CONST LPSOCKADDR)&lpRequestInfo->SocketAddress,
		sizeof(lpRequestInfo->SocketAddress),
		&lpRequestInfo->Overlapped,
		NULL);

	printf(__FUNCTION__ " - lpRequestInfo %p\n", lpRequestInfo);

	if (err == 0) // completed directly
		return TRUE;

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING) // Posted WSASendTo
		return TRUE;

	/*EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSASendTo, lpRequestInfo))
	Error(__FUNCTION__ " - %p - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);*/

	printf(__FUNCTION__ " - [ERROR] WSASendTo returned %d, code: %u\n", err, dwError);

	return FALSE;
}