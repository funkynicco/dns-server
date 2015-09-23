#pragma once

#define MAX_THREADS 32

typedef struct _tagDnsHeader
{
	// A 16-bit field identifying a specific DNS transaction.
	// The transaction ID is created by the message originator and is copied by the responder into its response message.
	u_short TransactionID;

	// A 16-bit field containing various service flags that are communicated between the DNS client and the DNS server, including:
	u_short Flags;

	// [Question Resource Record count] A 16-bit field representing the number of entries in the question section of the DNS message.
	u_short NumberOfQuestions;

	// [Answer Resource Record count] A 16-bit field representing the number of entries in the answer section of the DNS message.
	u_short NumberOfAnswers;

	// [Authority Resource Record count] A 16-bit field representing the number of authority resource records in the DNS message.
	u_short NumberOfAuthorities;

	// [Additional Resource Record count] A 16-bit field representing the number of additional resource records in the DNS message.
	u_short NumberOfAdditional;

} DNS_HEADER, *LPDNS_HEADER;

typedef struct _tagNetworkBuffer
{
	WSAOVERLAPPED Overlapped;

	struct _tagRequestInfo* lpRequestInfo;
	char Buffer[1024];
	DWORD dwLength;
	DWORD dwFlags;

	struct _tagNetworkBuffer* next;

} NETWORK_BUFFER, *LPNETWORK_BUFFER;

typedef struct _tagRequestInfo
{
	OVERLAPPED Overlapped;

	struct _tagServerInfo* lpServerInfo;
	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	struct _tagRequestInfo* next;

} REQUEST_INFO, *LPREQUEST_INFO;

typedef struct _tagRequestHandlerInfo
{
	HANDLE hIocp;
	
	DWORD dwNumberOfThreads;
	HANDLE hThreads[MAX_THREADS];

} REQUEST_HANDLER_INFO, *LPREQUEST_HANDLER_INFO;

typedef struct _tagServerInfo
{
	SOCKET Socket;

	REQUEST_HANDLER_INFO RequestHandler;

	LPREQUEST_INFO lpFreeRequests;
	LPNETWORK_BUFFER lpFreeNetworkBuffers;
	CRITICAL_SECTION csAllocRequest;

	// Network server
	HANDLE hNetworkIocp;
	HANDLE hNetworkThreads[MAX_THREADS];
	DWORD dwNumberOfNetworkThreads;

} SERVER_INFO, *LPSERVER_INFO;

/***************************************************************************************
 * Allocation.c
 ***************************************************************************************/

LPSERVER_INFO		AllocateServerInfo();
void				DestroyServerInfo(LPSERVER_INFO lpServerInfo);

LPREQUEST_INFO		AllocateRequestInfo(LPSERVER_INFO lpServerInfo);
void				DestroyRequestInfo(LPREQUEST_INFO lpRequestInfo);

LPNETWORK_BUFFER	AllocateNetworkBuffer(LPREQUEST_INFO lpRequestInfo);
void				DestroyNetworkBuffer(LPNETWORK_BUFFER lpNetworkBuffer);

/***************************************************************************************
 * Server.c
 ***************************************************************************************/

BOOL StartServer(LPSERVER_INFO lpServerInfo, int port);
void StopServer(LPSERVER_INFO lpServerInfo);
void ProcessServer(LPSERVER_INFO lpServerInfo);

/***************************************************************************************
 * RequestHandler.c
 ***************************************************************************************/

BOOL RequestHandlerInitialize(LPSERVER_INFO lpServerInfo, DWORD dwNumberOfThreads);
void RequestHandlerShutdown(LPSERVER_INFO lpServerInfo);

void RequestHandlerPostRequest(LPREQUEST_INFO lpRequestInfo);

/***************************************************************************************
 * ServerIO.c
 ***************************************************************************************/

BOOL ServerPostReceive(LPNETWORK_BUFFER lpNetworkBuffer);
BOOL ServerPostSend(LPNETWORK_BUFFER lpNetworkBuffer);