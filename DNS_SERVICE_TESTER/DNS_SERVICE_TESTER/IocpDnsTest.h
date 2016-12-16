#pragma once

enum
{
	IO_RECV,
	IO_SEND
};

typedef struct _tagRequestInfo
{
	OVERLAPPED Overlapped;

	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	char Buffer[1024];
	DWORD dwLength;
	DWORD dwFlags;

	int IOMode;
	SOCKET Socket;

	HANDLE hWaitObject;

	struct _tagRequestInfo* next;

} REQUEST_INFO, *LPREQUEST_INFO;

class IocpDnsTest
{
public:
	IocpDnsTest();
	virtual ~IocpDnsTest();

	BOOL ResolveDomain(const char* domain, vector<string>& result, double* pdResponseTime, DWORD dwMaxWaitTime = 3000);

private:
	LPREQUEST_INFO AllocateRequestInfo(SOCKET Socket, LPDWORD lpdwAllocatedRequests);
	void FreeRequestInfo(LPREQUEST_INFO lpRequestInfo);
	BOOL PostReceive(LPREQUEST_INFO lpRequestInfo, int IOMode);
	BOOL PostSend(LPREQUEST_INFO lpRequestInfo, int IOMode);
	void ProcessResponse(LPREQUEST_INFO lpRequestInfo);

	friend static DWORD WINAPI IocpThread(LPVOID lp);
	DWORD WorkerThread();
	SOCKET m_socket;
	HANDLE m_hThread;
	HANDLE m_hIocp;
	LARGE_INTEGER m_liFrequency;
	CRITICAL_SECTION m_cs;

	LPREQUEST_INFO m_lpRequestResponseBuckets[64];
	u_short m_requestResponseBucketIndexPool[64];
	DWORD m_dwRequestResponseBucketIndexPoolCount;

	LPREQUEST_INFO m_lpFreeRequests;
	CRITICAL_SECTION m_csAlloc;
	DWORD m_dwAllocatedRequests;
	DataLogger m_dataLogger;
};