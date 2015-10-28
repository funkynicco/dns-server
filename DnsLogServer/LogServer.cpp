#include "StdAfx.h"
#include "LogServer.h"

static DWORD WINAPI LogServerThread(LPVOID lp)
{
	return static_cast<LogServer*>(lp)->LogServerThread();
}

LogServer::LogServer() :
	m_socket(INVALID_SOCKET),
	m_dwNumberOfThreads(0),
	m_hIocp(NULL),
	m_bStarted(FALSE)
{
}

LogServer::~LogServer()
{
	Stop();
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
	if (!PostSend(lpBufferInfo, IO_RECV))
	{
		Stop();
		printf(__FUNCTION__ " - Failure in initial PostSend\n");
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
				printf(__FUNCTION__ " - Waited 3 sec for thread %08x\n", (ULONG_PTR)m_hThreads[i]);
		}
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
	
}

void LogServer::DestroyBufferInfo(LPBUFFER_INFO lpBufferInfo)
{

}