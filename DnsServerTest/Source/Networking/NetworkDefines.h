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

#define MAX_LOG_MESSAGE 384

enum
{
	LOGFLAG_NONE,
	LOGFLAG_SET_LAST_ID
};

typedef struct _tagLogMessage
{
	DWORD dwThread;
	int Flags; // LOGFLAG_*
	LONGLONG Id;
	time_t tmTime;
	DWORD dwLength;
	char Message[MAX_LOG_MESSAGE]; // MAX_LOG_MESSAGE is including the \0, so must be accounted for!

} LOG_MESSAGE, *LPLOG_MESSAGE;

#define MIN_LOG_MESSAGE_SIZE ((ULONG_PTR)&(((LPLOG_MESSAGE)0)->Message))