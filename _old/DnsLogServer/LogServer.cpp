#include "StdAfx.h"
#include "LogServer.h"

static DWORD WINAPI LogServerThread(LPVOID lp)
{
	return static_cast<LogServer*>(lp)->LogServerThread();
}

static DWORD WINAPI LogProcessThread(LPVOID lp)
{
	return static_cast<LogServer*>(lp)->LogProcessThread();
}

LogServer::LogServer() :
	m_socket(INVALID_SOCKET),
	m_dwNumberOfThreads(0),
	m_hIocp(NULL),
	m_bStarted(FALSE),
	m_pFreeBufferInfo(NULL),
	m_dwAllocatedBuffers(0),
	m_pCurrentMessageBuffer(NULL),
	m_pFreeMessageBuffers(NULL),
	m_lLastMessageIndex(0),
	m_hProcessThread(NULL),
	m_hProcessIocp(NULL),
	m_pFreeMessageProcessingObject(NULL),
	m_dwAllocatedMessageProcessingObjects(0)
{
	InitializeCriticalSectionAndSpinCount(&m_csAllocBuffer, 2000);
	InitializeCriticalSectionAndSpinCount(&m_csStats, 2000);
	InitializeCriticalSectionAndSpinCount(&m_csMessageBuffers, 2000);
	InitializeCriticalSectionAndSpinCount(&m_csAllocMessageProcessingObject, 2000);
}

LogServer::~LogServer()
{
	Stop();
	DeleteCriticalSection(&m_csAllocMessageProcessingObject);
	DeleteCriticalSection(&m_csMessageBuffers);
	DeleteCriticalSection(&m_csStats);
	DeleteCriticalSection(&m_csAllocBuffer);
}

BOOL LogServer::Start(int port)
{
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	return Start(&addr);
}

BOOL LogServer::Start(LPSOCKADDR_IN lpAddr)
{
	if (m_socket != INVALID_SOCKET)
		return FALSE;

	m_socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == INVALID_SOCKET)
		return FALSE;

	if (bind(m_socket, (LPSOCKADDR)lpAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		Stop();
		return FALSE;
	}

	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!m_hIocp)
	{
		Stop();
		return FALSE;
	}

	if (CreateIoCompletionPort((HANDLE)m_socket, m_hIocp, 0, 0) != m_hIocp)
	{
		Stop();
		return FALSE;
	}

	m_hProcessIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (!m_hProcessIocp)
	{
		Stop();
		return FALSE;
	}

	m_hProcessThread = CreateThread(NULL, 0, ::LogProcessThread, this, 0, NULL);
	if (!m_hProcessIocp)
	{
		Stop();
		return FALSE;
	}

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	for (DWORD i = 0; i < systemInfo.dwNumberOfProcessors; ++i)
	{
		m_hThreads[i] = CreateThread(NULL, 0, ::LogServerThread, this, CREATE_SUSPENDED, NULL);
		if (!m_hThreads[i])
		{
			Stop();
			return FALSE;
		}

		m_dwNumberOfThreads = i + 1;
	}

	for (DWORD i = 0; i < m_dwNumberOfThreads; ++i)
	{
		ResumeThread(m_hThreads[i]);
	}

	LPBUFFER_INFO lpBufferInfo = AllocateBufferInfo(m_socket);
	if (!PostReceive(lpBufferInfo, IO_RECV))
	{
		Stop();
		printf(__FUNCTION__ " - Failure in initial PostReceive\n");
		return FALSE;
	}

	m_bStarted = TRUE;
	return TRUE;
}

void LogServer::Stop()
{
	if (m_bStarted)
	{
		// kill any outstanding I/O....

		for (DWORD i = 0; i < m_dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(m_hIocp, 0, 0, NULL);

		for (DWORD i = 0; i < m_dwNumberOfThreads; ++i)
		{
			if (WaitForSingleObject(m_hThreads[i], 3000) == WAIT_TIMEOUT)
				printf(__FUNCTION__ " - Waited 3 sec for thread %p\n", m_hThreads[i]);
		}
	}

	if (m_hProcessThread)
	{
		PostQueuedCompletionStatus(m_hProcessIocp, 0, 0, NULL);
		if (WaitForSingleObject(m_hProcessThread, 3000) == WAIT_TIMEOUT)
			printf(__FUNCTION__ " - Waited 3 sec for processing thread %p\n", m_hProcessThread);
		CloseHandle(m_hProcessThread);
		m_hProcessThread = NULL;
	}

	if (m_hProcessIocp)
	{
		CloseHandle(m_hProcessIocp);
		m_hProcessIocp = NULL;
	}

	for (DWORD i = 0; i < m_dwNumberOfThreads; ++i)
	{
		if (m_hThreads[i])
		{
			CloseHandle(m_hThreads[i]);
			m_hThreads[i] = NULL;
		}
	}
	m_dwNumberOfThreads = 0;

	if (m_hIocp)
	{
		CloseHandle(m_hIocp);
		m_hIocp = NULL;
	}

	SAFE_CLOSE_SOCKET(m_socket);
	m_bStarted = FALSE;
}

LPBUFFER_INFO LogServer::AllocateBufferInfo(SOCKET Socket)
{
	EnterCriticalSection(&m_csAllocBuffer);
	LPBUFFER_INFO lpBufferInfo = m_pFreeBufferInfo;
	if (lpBufferInfo)
		m_pFreeBufferInfo = m_pFreeBufferInfo->next;
	DWORD dwAllocatedBuffers = ++m_dwAllocatedBuffers;
	LeaveCriticalSection(&m_csAllocBuffer);

	if (!lpBufferInfo)
		lpBufferInfo = (LPBUFFER_INFO)((char*)malloc(sizeof(BUFFER_INFO) + 1) + 1);

	EnterCriticalSection(&m_csStats);
	m_allocatedBuffers.insert(lpBufferInfo);
	LeaveCriticalSection(&m_csStats);

	ClearDestroyed(lpBufferInfo);

	ZeroMemory(lpBufferInfo, sizeof(BUFFER_INFO));
	lpBufferInfo->pLogServer = this;
	lpBufferInfo->Socket = Socket;
	return lpBufferInfo;
}

void LogServer::DestroyBufferInfo(LPBUFFER_INFO lpBufferInfo)
{
	if (IsDestroyed(lpBufferInfo))
	{
		printf(__FUNCTION__ " - DESTROYING ALREADY DESTROYED OBJECT %p", lpBufferInfo);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpBufferInfo);

	EnterCriticalSection(&m_csStats);
	m_allocatedBuffers.erase(lpBufferInfo);
	LeaveCriticalSection(&m_csStats);

	EnterCriticalSection(&m_csAllocBuffer);
	if (m_dwAllocatedBuffers == 0)
	{
		int* a = 0;
		*a = 1;
	}

	lpBufferInfo->next = m_pFreeBufferInfo;
	m_pFreeBufferInfo = lpBufferInfo;
	--m_dwAllocatedBuffers;
	LeaveCriticalSection(&m_csAllocBuffer);
}

void LogServer::ProcessMessageBuffer(LPMESSAGE_BUFFER lpMessageBuffer)
{
	for (int i = 0; i <= int(lpMessageBuffer->LastId - lpMessageBuffer->FirstId); ++i)
	{
		LPLOG_MESSAGE lpMessage = (LPLOG_MESSAGE)&lpMessageBuffer->Buffers[i]->Buffer[0];

		//lpMessage->Message[lpMessage->dwLength] = 0; // client isnt required to send \0


		EnterCriticalSection(&m_csAllocMessageProcessingObject);
		LPMESSAGE_PROCESSING_OBJECT lpMessageProcessingObject = m_pFreeMessageProcessingObject;
		if (lpMessageProcessingObject)
			m_pFreeMessageProcessingObject = m_pFreeMessageProcessingObject->next;
		LeaveCriticalSection(&m_csAllocMessageProcessingObject);

		char filename[MAX_PATH] = { 0 };
		char lineStr[16] = { 0 };
		//char threadStr[16] = { 0 };
		//char timeStr[32] = { 0 };

		const char* mark = lpMessage->Message;
		const char* ptr = lpMessage->Message;
		const char* end = lpMessage->Message + lpMessage->dwLength;
		int numToken = 0;
		while (ptr < end)
		{
			if (*ptr == ';')
			{
				++numToken;
				if (numToken == 1)
				{
					memcpy(filename, mark, ptr - mark);
					filename[ptr - mark] = 0;
					mark = ++ptr;
					continue;
				}
				else if (numToken == 2)
				{
					memcpy(lineStr, mark, ptr - mark);
					lineStr[ptr - mark] = 0;
					mark = ++ptr;
					break;
				}
				//else if (numToken == 3)
				//{
				//	memcpy(threadStr, mark, ptr - mark);
				//	threadStr[ptr - mark] = 0;
				//	mark = ++ptr;
				//	continue;
				//}
				//else if (numToken == 4)
				//{
				//	memcpy(timeStr, mark, ptr - mark);
				//	timeStr[ptr - mark] = 0;
				//	mark = ++ptr;
				//	break;
				//}
			}

			++ptr;
		}

		if (!lpMessageProcessingObject)
			lpMessageProcessingObject = (LPMESSAGE_PROCESSING_OBJECT)malloc(sizeof(MESSAGE_PROCESSING_OBJECT));

		lpMessageProcessingObject->LogItem.LogMessage.dwThread = lpMessage->dwThread;
		lpMessageProcessingObject->LogItem.LogMessage.Flags = lpMessage->Flags;
		lpMessageProcessingObject->LogItem.LogMessage.Id = lpMessage->Id;
		lpMessageProcessingObject->LogItem.LogMessage.tmTime = lpMessage->tmTime;

		memcpy(lpMessageProcessingObject->LogItem.LogMessage.Message, ptr, end - ptr);
		lpMessageProcessingObject->LogItem.LogMessage.Message[end - ptr] = 0;

		strcpy(lpMessageProcessingObject->LogItem.Filename, filename);

		if (*lineStr)
			lpMessageProcessingObject->LogItem.Line = atoi(lineStr);
		else
			lpMessageProcessingObject->LogItem.Line = 1;

		//if (*threadStr)
		//	lpMessageProcessingObject->LogItem.dwThread = (DWORD)atoi(threadStr);
		//else
		//	lpMessageProcessingObject->LogItem.dwThread = 1;

		if (!PostQueuedCompletionStatus(m_hProcessIocp, 0, 0, &lpMessageProcessingObject->Overlapped))
			printf(__FUNCTION__ " - PostQueuedCompletionStatus failed: %s\n", GetErrorMessage(GetLastError()));
	}
}