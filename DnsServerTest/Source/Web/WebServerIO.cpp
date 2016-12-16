#include "StdAfx.h"
#include "WebServer.h"

/*
EnterCriticalSection(&lpServerInfo->csStats);
if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSARecv, lpBuffer))
Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecv", (ULONG_PTR)lpBuffer, GetIOMode(lpBuffer->IOMode));
LeaveCriticalSection(&lpServerInfo->csStats);
*/

#define TryPostOrDestroy(_Function, _Buffer, _IOMode) \
	if (!_Function(_Buffer, _IOMode)) \
	{ \
		Error(__FUNCTION__ " - " #_Function " returned FALSE, NumberOfBuffers: %u", _Buffer->lpClientInfo->NumberOfBuffers); \
		if (InterlockedDecrement(&_Buffer->lpClientInfo->NumberOfBuffers) == 0) \
		{ \
			Error(__FUNCTION__ " WARNING: remove client %p", lpClientInfo); \
			SocketPoolDestroySocket(SOCKETTYPE_TCP, lpClientInfo->Socket); \
			DestroyWebClientInfo(lpClientInfo); \
		} \
	}

BOOL WebServerPostReceive(LPWEB_CLIENT_BUFFER lpBuffer, int IOMode)
{
    LPWEB_CLIENT_INFO lpClientInfo = lpBuffer->lpClientInfo;
    LPWEB_SERVER_INFO lpServerInfo = lpClientInfo->lpServerInfo;

    if (lpClientInfo->bFreeze)
        return FALSE;

#ifdef __LOG_WEB_SERVER_IO
    char addrtext[16];
    GetIPFromSocketAddress(&lpClientInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_WEB_SERVER_IO

    WSABUF wsaBuf;
    wsaBuf.buf = lpBuffer->Buffer;
    wsaBuf.len = sizeof(lpBuffer->Buffer);

    lpBuffer->dwFlags = 0;
    lpBuffer->IOMode = IOMode;

    SafeArrayContainerAddElement(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer);

    int err = WSARecv(
        lpClientInfo->Socket,
        &wsaBuf,
        1,
        NULL,
        &lpBuffer->dwFlags,
        &lpBuffer->Overlapped,
        NULL);

    if (err == 0)
    {
        // completed directly
#ifdef __LOG_WEB_SERVER_IO
        LoggerWrite(
            __FUNCTION__ " - WARNING - WSARecv completed directly - Socket: %u, lpBuffer: %p, IOMode: %s, target: %s",
            lpClientInfo->Socket,
            (ULONG_PTR)lpBuffer,
            GetIOMode(lpBuffer->IOMode),
            addrtext);
#endif // __LOG_WEB_SERVER_IO
        return TRUE;
    }

    DWORD dwError = WSAGetLastError();

    if (dwError == WSA_IO_PENDING)
    {
#ifdef __LOG_WEB_SERVER_IO
        LoggerWrite(
            __FUNCTION__ " - Posted WSARecv - Socket: %u, lpBuffer: %p, IOMode: %s, target: %s",
            lpClientInfo->Socket,
            (ULONG_PTR)lpBuffer,
            GetIOMode(lpBuffer->IOMode),
            addrtext);
#endif // __LOG_WEB_SERVER_IO
        return TRUE;
    }

    SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer);

#ifndef __LOG_WEB_SERVER_IO
    char addrtext[16];
    GetIPFromSocketAddress(&lpClientInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_WEB_SERVER_IO

    Error(__FUNCTION__ " - [ERROR] WSARecv [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

    return FALSE;
}

BOOL WebServerPostSend(LPWEB_CLIENT_BUFFER lpBuffer, int IOMode)
{
    LPWEB_CLIENT_INFO lpClientInfo = lpBuffer->lpClientInfo;
    LPWEB_SERVER_INFO lpServerInfo = lpClientInfo->lpServerInfo;

#ifdef __LOG_WEB_SERVER_IO
    char addrtext[16];
    GetIPFromSocketAddress(&lpClientInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_WEB_SERVER_IO

    WSABUF wsaBuf;
    wsaBuf.buf = lpBuffer->Buffer;
    wsaBuf.len = lpBuffer->dwLength;

    lpBuffer->IOMode = IOMode;

    SafeArrayContainerAddElement(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer);

    int err = WSASend(
        lpClientInfo->Socket,
        &wsaBuf,
        1,
        NULL,
        0,
        &lpBuffer->Overlapped,
        NULL);

    if (err == 0)
    {
        // completed directly, do we need to do anything here?
#ifdef __LOG_WEB_SERVER_IO
        LoggerWrite(
            __FUNCTION__ " - WARNING - WSASend completed directly - Socket: %u, lpBuffer: %p, IOMode: %s, target: %s",
            lpClientInfo->Socket,
            (ULONG_PTR)lpBuffer,
            GetIOMode(lpBuffer->IOMode),
            addrtext);
#endif // __LOG_WEB_SERVER_IO
        return TRUE;
    }

    DWORD dwError = WSAGetLastError();

    if (dwError == WSA_IO_PENDING)
    {
#ifdef __LOG_WEB_SERVER_IO
        LoggerWrite(
            __FUNCTION__ " - Posted WSASend - Socket: %u, lpBuffer: %p, IOMode: %s, target: %s",
            lpClientInfo->Socket,
            (ULONG_PTR)lpBuffer,
            GetIOMode(lpBuffer->IOMode),
            addrtext);
#endif // __LOG_WEB_SERVER_IO
        return TRUE;
    }

    SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer);

#ifndef __LOG_WEB_SERVER_IO
    char addrtext[16];
    GetIPFromSocketAddress(&lpClientInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_WEB_SERVER_IO

    Error(__FUNCTION__ " - [ERROR] WSASend [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

    return FALSE;
}

BOOL WebServerPostAccept(LPWEB_SERVER_INFO lpServerInfo)
{
    if (lpServerInfo->bOutstandingAccept)
        return FALSE;

    lpServerInfo->AcceptContext.Socket = SocketPoolAllocateSocket(SOCKETTYPE_TCP);
    if (lpServerInfo->AcceptContext.Socket == INVALID_SOCKET)
        return FALSE;

    lpServerInfo->bOutstandingAccept = TRUE;
    if (!lpServerInfo->lpfnAcceptEx(
        lpServerInfo->ListenSocket,
        lpServerInfo->AcceptContext.Socket,
        lpServerInfo->AcceptContext.AcceptBuffer,
        0,
        sizeof(SOCKADDR_IN) + 16,
        sizeof(SOCKADDR_IN) + 16,
        &lpServerInfo->AcceptContext.dwBytesReceived,
        (LPOVERLAPPED)&lpServerInfo->AcceptContext) &&
        GetLastError() != WSA_IO_PENDING)
    {
        DWORD dwError = GetLastError();
        Error(__FUNCTION__ " - [ERROR] AcceptEx returned false, code: %u - %s", dwError, GetErrorMessage(dwError));
        return FALSE;
    }

#ifdef __LOG_WEB_SERVER_IO
    LoggerWrite(__FUNCTION__ " - Posted WSAAccept - Socket: %u", lpServerInfo->AcceptContext.Socket);
#endif // __LOG_WEB_SERVER_IO

    return TRUE;
}

/***************************************************************************************
 * WebServerThread
 ***************************************************************************************/

DWORD WINAPI WebServerThread(LPVOID lp)
{
    LPWEB_SERVER_INFO lpServerInfo = (LPWEB_SERVER_INFO)lp;

    LPOVERLAPPED lpOverlapped;
    DWORD dwBytesTransferred;
    ULONG_PTR ulCompletionKey;

#ifdef __LOG_WEB_SERVER_IO
    char addrtext[16];
#endif // __LOG_WEB_SERVER_IO

    for (;;)
    {
        if (!GetQueuedCompletionStatus(
            lpServerInfo->IOThreads.hIocp,
            &dwBytesTransferred,
            &ulCompletionKey,
            &lpOverlapped,
            INFINITE))
        {
            DWORD dwError = GetLastError();
            //LoggerWrite(__FUNCTION__ " - [FAILED] GetQueuedCompletionStatus returned FALSE: %s", GetErrorMessage(dwError));
            //LoggerWrite(__FUNCTION__ " - FAILURE INFO[dw: %u, key: %u, lp: %p]", dwBytesTransferred, ulCompletionKey, (ULONG_PTR)lpOverlapped);

            switch (dwError)
            {
            case ERROR_CONNECTION_ABORTED:
            case ERROR_NETNAME_DELETED:
            case WSAENOTSOCK:
                if (lpOverlapped != &lpServerInfo->AcceptContext.Overlapped)
                {
                    LPWEB_CLIENT_BUFFER lpBuffer = (LPWEB_CLIENT_BUFFER)lpOverlapped;
                    LPWEB_CLIENT_INFO lpClientInfo = lpBuffer->lpClientInfo;

                    Error(__FUNCTION__ " I/O failed - lpBuffer: %p, remaining buffers: %u", lpBuffer, lpClientInfo->NumberOfBuffers);

                    switch (lpBuffer->IOMode)
                    {
                    case IO_RECV: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer); break;
                    case IO_SEND: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer); break;
                    }

                    DestroyWebClientBuffer(lpBuffer);

                    // something happened with the socket and we must push back all the buffers
                    //lpBuffer->lpClientInfo->--dwBuffers <-- interlocked decrement, when zero we destroy client too
                    if (InterlockedDecrement(&lpClientInfo->NumberOfBuffers) == 0)
                    {
                        // remove client
                        Error(__FUNCTION__ " WARNING: remove client %p", lpClientInfo);
                        SocketPoolDestroySocket(SOCKETTYPE_TCP, lpClientInfo->Socket);
                        DestroyWebClientInfo(lpClientInfo);
                    }
                }
                else
                    Error(__FUNCTION__ " - I/O failed - AcceptContext dwError: %s", GetErrorMessage(dwError));
                break;
            case ERROR_OPERATION_ABORTED:
                if (lpOverlapped)
                {
                    if (lpOverlapped == &lpServerInfo->AcceptContext.Overlapped)
                    {
                        Error(__FUNCTION__ " - Canceled WSAAccept (socket %Id)", lpServerInfo->ListenSocket);
                        lpServerInfo->bOutstandingAccept = FALSE;
                        SocketPoolDestroySocket(SOCKETTYPE_TCP, lpServerInfo->AcceptContext.Socket);
                    }
                    else
                    {
                        LPWEB_CLIENT_BUFFER lpBuffer = (LPWEB_CLIENT_BUFFER)lpOverlapped;
                        LPWEB_CLIENT_INFO lpClientInfo = (LPWEB_CLIENT_INFO)lpBuffer->lpClientInfo;

#ifdef __LOG_WEB_SERVER_IO
                        LoggerWrite(
                            __FUNCTION__ " - I/O request %p canceled, IOMode: %s, Socket: %Id",
                            lpBuffer,
                            GetIOMode(lpBuffer->IOMode),
                            lpBuffer->lpClientInfo->Socket);
#endif // __LOG_WEB_SERVER_IO

                        switch (lpBuffer->IOMode)
                        {
                        case IO_RECV: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer); break;
                        case IO_SEND: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer); break;
                        }

                        DestroyWebClientBuffer(lpBuffer);

                        if (InterlockedDecrement(&lpClientInfo->NumberOfBuffers) == 0)
                        {
                            // remove client
                            Error(__FUNCTION__ " WARNING: remove client %p", lpClientInfo);
                            SocketPoolDestroySocket(SOCKETTYPE_TCP, lpClientInfo->Socket);
                            DestroyWebClientInfo(lpClientInfo);
                        }
                    }
                }
                break;
            default:
                Error(
                    __FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
                    "(dwBytesTransferred: %d, ulCompletionKey: %Id, lpOverlapped: %p)\r\n%s",
                    GetLastError(),
                    dwBytesTransferred,
                    ulCompletionKey,
                    lpOverlapped,
                    GetErrorMessage(GetLastError()));
                break;
            }

            continue;
        }

        if (dwBytesTransferred == 0 &&
            ulCompletionKey == 0 &&
            lpOverlapped == NULL)
            break; // the owner is killing all ServerIO threads

        SOCKET Socket;
        if (lpOverlapped != &lpServerInfo->AcceptContext.Overlapped)
            Socket = ((LPWEB_CLIENT_BUFFER)lpOverlapped)->lpClientInfo->Socket;
        else
            Socket = lpServerInfo->ListenSocket;

#ifdef __LOG_WEB_SERVER_IO
        int IOMode = IO_ACCEPT;
        if (lpOverlapped != &lpServerInfo->AcceptContext.Overlapped)
        {
            GetIPFromSocketAddress(&((LPWEB_CLIENT_BUFFER)lpOverlapped)->lpClientInfo->SocketAddress, addrtext, sizeof(addrtext));
            IOMode = ((LPWEB_CLIENT_BUFFER)lpOverlapped)->IOMode;
        }
        else
            strcpy(addrtext, "AcceptEx");

        LoggerWrite(
            __FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpOverlapped: %p, IOMode: %s, Socket: %Id [from %s]",
            dwBytesTransferred,
            ulCompletionKey,
            lpOverlapped,
            GetIOMode(IOMode),
            Socket,
            addrtext);
#endif // __LOG_WEB_SERVER_IO

        if (lpOverlapped != &lpServerInfo->AcceptContext.Overlapped)
        {
            LPWEB_CLIENT_BUFFER lpBuffer = (LPWEB_CLIENT_BUFFER)lpOverlapped;
            LPWEB_CLIENT_INFO lpClientInfo = lpBuffer->lpClientInfo;

            if (dwBytesTransferred == 0)
            {
                lpClientInfo->bFreeze = TRUE;

                // issue
                switch (lpBuffer->IOMode)
                {
                case IO_RECV: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer); break;
                case IO_SEND: SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer); break;
                }

                DestroyWebClientBuffer(lpBuffer);

                if (InterlockedDecrement(&lpClientInfo->NumberOfBuffers) == 0)
                {
                    // remove client
                    Error(__FUNCTION__ " WARNING: (client connection closed) remove client %p", lpClientInfo);
                    SocketPoolDestroySocket(SOCKETTYPE_TCP, lpClientInfo->Socket);
                    DestroyWebClientInfo(lpClientInfo);
                }

                continue;
            }

            switch (lpBuffer->IOMode)
            {
            case IO_RECV:
            {
                SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSARecv, lpBuffer);

                lpBuffer->dwLength = dwBytesTransferred;

                // TODO: ... read data...
                char textbuf[4096];
                char* tptr = textbuf;
                for (DWORD i = 0; i < lpBuffer->dwLength; ++i)
                {
                    /*if (i > 0)
                    {
                        if(i%16)
                    }*/

                    tptr += sprintf(tptr, "%c", lpBuffer->Buffer[i]);
                }
                *tptr = 0;

                LoggerWrite(__FUNCTION__ " - [from %s, lpBuffer: %p] Received %u bytes: %s", addrtext, lpBuffer, lpBuffer->dwLength, textbuf);

                char str[512];
                int str_len = sprintf(str, "request OK - t: %llu", time(NULL));

                LPWEB_CLIENT_BUFFER lpSendBuffer1 = AllocateWebClientBuffer(lpBuffer->lpClientInfo);
                LPWEB_CLIENT_BUFFER lpSendBuffer2 = AllocateWebClientBuffer(lpBuffer->lpClientInfo);

                InterlockedIncrement(&lpClientInfo->NumberOfBuffers);
                InterlockedIncrement(&lpClientInfo->NumberOfBuffers);

                lpSendBuffer1->dwLength = sprintf(
                    lpSendBuffer1->Buffer,
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n",
                    str_len);

                lpSendBuffer2->dwLength = str_len;
                memcpy(lpSendBuffer2->Buffer, str, str_len);

                // fragmented packets cause yolo
                TryPostOrDestroy(WebServerPostSend, lpSendBuffer1, IO_SEND);
                //Sleep(500); // yolo even more
                TryPostOrDestroy(WebServerPostSend, lpSendBuffer2, IO_SEND);

                TryPostOrDestroy(WebServerPostReceive, lpBuffer, IO_RECV);
            }
            break;
            case IO_SEND:
                SafeArrayContainerDeleteElementByValue(&lpServerInfo->csStats, &lpServerInfo->PendingWSASend, lpBuffer);

                // this was a completed send request
#ifdef __LOG_WEB_ALLOCATIONS
                LoggerWrite(__FUNCTION__ " - Destroyed %s lpBuffer : %p", GetIOMode(lpBuffer->IOMode), lpBuffer);
#endif // __LOG_WEB_ALLOCATIONS

                DestroyWebClientBuffer(lpBuffer);

                if (InterlockedDecrement(&lpClientInfo->NumberOfBuffers) == 0)
                {
                    // remove client
                    Error(__FUNCTION__ " WARNING: (client connection closed) remove client %p", lpClientInfo);
                    SocketPoolDestroySocket(SOCKETTYPE_TCP, lpClientInfo->Socket);
                    DestroyWebClientInfo(lpClientInfo);
                }

                break;
            }
        }
        else
        {
            lpServerInfo->bOutstandingAccept = FALSE;

            // accept
            LPSOCKADDR_IN lpLocalSockaddr;
            int localSockaddrLength = sizeof(lpLocalSockaddr);
            LPSOCKADDR_IN lpRemoteSockaddr;
            int remoteSockaddrLength = sizeof(lpRemoteSockaddr);

            lpServerInfo->lpfnGetAcceptExSockaddrs(
                lpServerInfo->AcceptContext.AcceptBuffer,
                0,
                sizeof(SOCKADDR_IN) + 16,
                sizeof(SOCKADDR_IN) + 16,
                (LPSOCKADDR*)&lpLocalSockaddr,
                &localSockaddrLength,
                (LPSOCKADDR*)&lpRemoteSockaddr,
                &remoteSockaddrLength);

            Error(__FUNCTION__ " - Client accepted from %d.%d.%d.%d",
                lpRemoteSockaddr->sin_addr.s_addr & 0xff,
                (lpRemoteSockaddr->sin_addr.s_addr >> 8) & 0xff,
                (lpRemoteSockaddr->sin_addr.s_addr >> 16) & 0xff,
                (lpRemoteSockaddr->sin_addr.s_addr >> 24) & 0xff);

            // TODO: create client etc here

            if (CreateIoCompletionPort(
                (HANDLE)lpServerInfo->AcceptContext.Socket,
                lpServerInfo->IOThreads.hIocp,
                lpServerInfo->AcceptContext.Socket, 0) == lpServerInfo->IOThreads.hIocp)
            {
                LPWEB_CLIENT_INFO lpClientInfo = AllocateWebClientInfo(lpServerInfo, lpServerInfo->AcceptContext.Socket);
                CopyMemory(&lpClientInfo->SocketAddress, lpRemoteSockaddr, sizeof(SOCKADDR_IN));
                lpClientInfo->SockAddrLen = sizeof(lpClientInfo->SocketAddress);

                LPWEB_CLIENT_BUFFER lpBuffer = AllocateWebClientBuffer(lpClientInfo);
                InterlockedIncrement(&lpClientInfo->NumberOfBuffers);

                TryPostOrDestroy(WebServerPostReceive, lpBuffer, IO_RECV);
            }
            else
            {
                DWORD dwError = GetLastError();

                Error(__FUNCTION__ " - Failed to associate socket %Id with I/O completion port: %s",
                    lpServerInfo->AcceptContext.Socket,
                    GetErrorMessage(dwError));

                SocketPoolDestroySocket(SOCKETTYPE_TCP, lpServerInfo->AcceptContext.Socket);
            }

            WebServerPostAccept(lpServerInfo);
        }
    }

    return 0;
}