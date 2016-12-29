#pragma once

#include "Networking\NetworkDefines.h"

typedef struct _tagDnsHeader // size 14
{
	// A 16-bit field identifying a specific DNS transaction.
	// The transaction ID is created by the message originator and is copied by the responder into its response message.
	u_short TransactionID;

	// A 16-bit field containing various service flags that are communicated between the DNS client and the DNS server, including:
	/*union
	{
		struct
		{
			unsigned char QR : 1;
			unsigned char OPCODE : 4;
			unsigned char AA : 1;
			unsigned char TC : 1;
			unsigned char RD : 1;

			unsigned char RA : 1;
			unsigned char res1 : 1;
			unsigned char res2 : 1;
			unsigned char res3 : 1;
			unsigned char RCODE : 4;
		};*/

	u_short Flags;
	//} Flags;


	// [Question Resource Record count] A 16-bit field representing the number of entries in the question section of the DNS message.
	u_short NumberOfQuestions;

	// [Answer Resource Record count] A 16-bit field representing the number of entries in the answer section of the DNS message.
	u_short NumberOfAnswers;

	// [Authority Resource Record count] A 16-bit field representing the number of authority resource records in the DNS message.
	u_short NumberOfAuthorities;

	// [Additional Resource Record count] A 16-bit field representing the number of additional resource records in the DNS message.
	u_short NumberOfAdditional;

} DNS_HEADER, *LPDNS_HEADER;

typedef struct _tagDnsRequestTimeoutHandler DNS_REQUEST_TIMEOUT_HANDLER, *LPDNS_REQUEST_TIMEOUT_HANDLER;

typedef struct _tagDnsRequestInfo
{
	WSAOVERLAPPED Overlapped;

	struct _tagDnsServerInfo* lpServerInfo;
	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	char Buffer[1024];
	DWORD dwLength;
	DWORD dwFlags;

	int IOMode;
	SOCKET Socket;

    LARGE_INTEGER liTimeReceivedRequest;

	struct _tagDnsRequestInfo* lpInnerRequest;

	struct _tagDnsRequestInfo* next;

} DNS_REQUEST_INFO, *LPDNS_REQUEST_INFO;

typedef struct _tagDnsServerInfo
{
	SOCKET Socket;

	IO_THREADS_INFO RequestHandlerThreads;

	LPDNS_REQUEST_INFO lpFreeRequests;
	DWORD dwAllocatedRequests;
	CRITICAL_SECTION csAllocRequest;

	//DWORD dwPendingWSARecvFrom;
	//DWORD dwPendingWSASendTo;
	ARRAY_CONTAINER PendingWSARecvFrom;
	ARRAY_CONTAINER PendingWSASendTo;
	ARRAY_CONTAINER AllocatedRequests;
	CRITICAL_SECTION csStats;

	LPDNS_REQUEST_TIMEOUT_HANDLER lpRequestTimeoutHandler;

	SOCKADDR_IN SecondaryDnsServerSocketAddress;

	// Network server
	/*HANDLE hNetworkIocp;
	HANDLE hNetworkThreads[MAX_THREADS];
	DWORD dwNumberOfNetworkThreads;*/
	IO_THREADS_INFO NetworkServerThreads;

} DNS_SERVER_INFO, *LPDNS_SERVER_INFO;

/***************************************************************************************
* DnsRequestTimeoutHandler
***************************************************************************************/
LPDNS_REQUEST_TIMEOUT_HANDLER DnsRequestTimeoutCreate();
void DnsRequestTimeoutDestroy(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler);
void DnsRequestTimeoutSetCancelTimeout(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler, LPDNS_REQUEST_INFO lpRequestInfo, time_t tmTimeout);
void DnsRequestTimeoutRemoveRequest(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler, LPDNS_REQUEST_INFO lpRequestInfo);
void DnsRequestTimeoutProcess(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler);

/***************************************************************************************
 * DnsAllocation.c
 ***************************************************************************************/

LPDNS_SERVER_INFO		AllocateDnsServerInfo();
void					DestroyDnsServerInfo(LPDNS_SERVER_INFO lpServerInfo);

// If lpCopyOriginal is not NULL then it will copy all values from lpCopyOriginal into a new instance
LPDNS_REQUEST_INFO		AllocateDnsRequestInfo(LPDNS_SERVER_INFO lpServerInfo, SOCKET Socket);
LPDNS_REQUEST_INFO		CopyDnsRequestInfo(LPDNS_REQUEST_INFO lpOriginalRequestInfo);
void					DestroyDnsRequestInfo(LPDNS_REQUEST_INFO lpRequestInfo);

/***************************************************************************************
 * DnsServer.c
 ***************************************************************************************/

BOOL DnsServerStart(LPDNS_SERVER_INFO lpServerInfo);
void DnsServerStop(LPDNS_SERVER_INFO lpServerInfo);
void DnsServerSetSecondaryDnsServer(LPDNS_SERVER_INFO lpServerInfo, LPSOCKADDR_IN lpSockAddr);

/***************************************************************************************
 * DnsRequestHandler.c
 ***************************************************************************************/

BOOL DnsRequestHandlerInitialize(LPDNS_SERVER_INFO lpServerInfo, DWORD dwNumberOfThreads);
void DnsRequestHandlerShutdown(LPDNS_SERVER_INFO lpServerInfo);

void DnsRequestHandlerPostRequest(LPDNS_REQUEST_INFO lpRequestInfo);

/***************************************************************************************
 * DnsServerIO.c
 ***************************************************************************************/

BOOL DnsServerPostReceive(LPDNS_REQUEST_INFO lpRequestInfo, int IOMode);
BOOL DnsServerPostSend(LPDNS_REQUEST_INFO lpRequestInfo, int IOMode);

/***************************************************************************************
 * DnsResolve.c
 ***************************************************************************************/

BOOL InitializeDnsResolver(LPDNS_SERVER_INFO lpServerInfo);
void DestroyDnsResolver();
void ResolveDns(LPDNS_REQUEST_INFO lpRequestInfo);