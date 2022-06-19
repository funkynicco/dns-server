#pragma once

#define MAX_LOG_THREADS 32

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

#define MAX_MESSAGE_BUFFERS 32
typedef struct _tagMessageBuffer
{
	LPBUFFER_INFO Buffers[MAX_MESSAGE_BUFFERS];
	LONGLONG FirstId;
	LONGLONG LastId;

	struct _tagMessageBuffer* next;

} MESSAGE_BUFFER, *LPMESSAGE_BUFFER;

typedef struct _tagMessageProcessingObject
{
	OVERLAPPED Overlapped;

	LOG_ITEM LogItem;

	struct _tagMessageProcessingObject* next;

} MESSAGE_PROCESSING_OBJECT, *LPMESSAGE_PROCESSING_OBJECT;

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
	friend static DWORD WINAPI LogProcessThread(LPVOID lp);

	// functions
	LPBUFFER_INFO AllocateBufferInfo(SOCKET Socket);
	void DestroyBufferInfo(LPBUFFER_INFO lpBufferInfo);
	BOOL PostReceive(LPBUFFER_INFO lpBufferInfo, int IOMode);
	BOOL PostSend(LPBUFFER_INFO lpBufferInfo, int IOMode);
	DWORD LogServerThread();
	BOOL IsMessageBufferValid(LPMESSAGE_BUFFER lpMessageBuffer);
	void ProcessMessageBuffer(LPMESSAGE_BUFFER lpMessageBuffer);

	DWORD LogProcessThread();
	void ProcessMessageObject(LPMESSAGE_PROCESSING_OBJECT lpMessageProcessingObject);

	SOCKET m_socket;
	HANDLE m_hThreads[MAX_LOG_THREADS];
	DWORD m_dwNumberOfThreads;
	HANDLE m_hIocp;
	BOOL m_bStarted;

	CRITICAL_SECTION m_csAllocBuffer;
	LPBUFFER_INFO m_pFreeBufferInfo;
	DWORD m_dwAllocatedBuffers;

	CRITICAL_SECTION m_csStats;
	unordered_set<LPBUFFER_INFO> m_allocatedBuffers;

	CRITICAL_SECTION m_csMessageBuffers;
	LPMESSAGE_BUFFER m_pCurrentMessageBuffer;
	LPMESSAGE_BUFFER m_pFreeMessageBuffers;
	LONGLONG m_lLastMessageIndex;

	HANDLE m_hProcessThread;
	HANDLE m_hProcessIocp;

	CRITICAL_SECTION m_csAllocMessageProcessingObject;
	LPMESSAGE_PROCESSING_OBJECT m_pFreeMessageProcessingObject;
	DWORD m_dwAllocatedMessageProcessingObjects;

	Logger m_logger;
};

inline BOOL LogServer::IsMessageBufferValid(LPMESSAGE_BUFFER lpMessageBuffer)
{
	for (int i = 0; i <= int(lpMessageBuffer->LastId - lpMessageBuffer->FirstId); ++i)
	{
		if (!lpMessageBuffer->Buffers[i])
			return FALSE;
	}

	return TRUE;
}