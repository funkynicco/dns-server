#pragma once

#define MAX_LOG_THREADS 8

enum
{
	IO_RECV,
	IO_SEND,
	IO_RELAY_RECV,
	IO_RELAY_SEND,
	IO_ACCEPT
};

class LogServer;
typedef struct _tagBufferInfo
{
	WSAOVERLAPPED Overlapped;

	LogServer* pLogServer;
	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	char Buffer[1024];
	DWORD dwLength;
	DWORD dwFlags;

	int IOMode;
	SOCKET Socket;

	struct _tagBufferInfo* next;

} BUFFER_INFO, *LPBUFFER_INFO;

class LogServer
{
public:
	LogServer();
	virtual ~LogServer();

	BOOL Start(int port);
	BOOL Start(LPSOCKADDR_IN lpAddr);
	void Stop();

private:
	friend static DWORD WINAPI LogServerThread(LPVOID lp);
	
	// functions
	LPBUFFER_INFO AllocateBufferInfo(SOCKET Socket);
	void DestroyBufferInfo(LPBUFFER_INFO lpBufferInfo);
	BOOL PostReceive(LPBUFFER_INFO lpBufferInfo, int IOMode);
	BOOL PostSend(LPBUFFER_INFO lpBufferInfo, int IOMode);
	DWORD LogServerThread();

	SOCKET m_socket;
	HANDLE m_hThreads[MAX_LOG_THREADS];
	DWORD m_dwNumberOfThreads;
	HANDLE m_hIocp;
	BOOL m_bStarted;
};