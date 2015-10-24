#pragma once

#define MAX_THREADS 32

enum
{
	IO_RECV,
	IO_SEND,
	IO_RELAY_RECV,
	IO_RELAY_SEND,
	IO_ACCEPT
};

typedef struct _tagIOThreadsInfo
{
	HANDLE hIocp;

	DWORD dwNumberOfThreads;
	HANDLE hThreads[MAX_THREADS];

} IO_THREADS_INFO, *LPIO_THREADS_INFO;

typedef struct _tagAcceptContext
{
	OVERLAPPED Overlapped;
	SOCKET Socket;
	char AcceptBuffer[sizeof(SOCKADDR_IN) * 2 + 32];
	DWORD dwBytesReceived; // unused

} ACCEPT_CONTEXT, *LPACCEPT_CONTEXT;