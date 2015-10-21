#pragma once

#define MAX_THREADS 32

enum
{
	IO_RECV,
	IO_SEND,
	IO_RELAY_RECV,
	IO_RELAY_SEND
};

typedef struct _tagIOThreadsInfo
{
	HANDLE hIocp;

	DWORD dwNumberOfThreads;
	HANDLE hThreads[MAX_THREADS];

} IO_THREADS_INFO, *LPIO_THREADS_INFO;