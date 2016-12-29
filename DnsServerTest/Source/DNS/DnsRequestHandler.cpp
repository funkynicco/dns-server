#include "StdAfx.h"
#include "DnsServer.h"

DWORD WINAPI DnsRequestHandlerThread(LPVOID);

BOOL DnsRequestHandlerInitialize(LPDNS_SERVER_INFO lpServerInfo, DWORD dwNumberOfThreads)
{
    DnsRequestHandlerShutdown(lpServerInfo);

    if (dwNumberOfThreads > MAX_THREADS)
        return FALSE;

    LPIO_THREADS_INFO lpRequestHandlerThreads = &lpServerInfo->RequestHandlerThreads;

    lpRequestHandlerThreads->dwNumberOfThreads = 0;

    for (DWORD i = 0; i < dwNumberOfThreads; ++i)
    {
        lpRequestHandlerThreads->hThreads[i] = CreateThread(NULL, 0, DnsRequestHandlerThread, lpServerInfo, CREATE_SUSPENDED, NULL);
        if (!lpRequestHandlerThreads->hThreads[i])
        {
            Error(__FUNCTION__ " - Could not create thread (%d of %d): %d - %s",
                i + 1,
                dwNumberOfThreads,
                GetLastError(),
                GetErrorMessage(GetLastError()));
            DnsRequestHandlerShutdown(lpServerInfo);
            return FALSE;
        }
        ++lpRequestHandlerThreads->dwNumberOfThreads;
    }

    lpRequestHandlerThreads->hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwNumberOfThreads);
    if (!lpRequestHandlerThreads->hIocp)
    {
        Error(__FUNCTION__ " - Could not create IO completion port: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
        DnsRequestHandlerShutdown(lpServerInfo);
        return FALSE;
    }

    for (DWORD i = 0; i < lpRequestHandlerThreads->dwNumberOfThreads; ++i)
        ResumeThread(lpRequestHandlerThreads->hThreads[i]);

    return TRUE;
}

void DnsRequestHandlerShutdown(LPDNS_SERVER_INFO lpServerInfo)
{
    LPIO_THREADS_INFO lpRequestHandlerThreads = &lpServerInfo->RequestHandlerThreads;

    if (lpRequestHandlerThreads->hIocp)
    {
        for (DWORD i = 0; i < lpRequestHandlerThreads->dwNumberOfThreads; ++i)
            PostQueuedCompletionStatus(lpRequestHandlerThreads->hIocp, 0, 0, NULL);
    }

    for (DWORD i = 0; i < lpRequestHandlerThreads->dwNumberOfThreads; ++i)
    {
        if (WaitForSingleObject(lpRequestHandlerThreads->hThreads[i], 3000) == WAIT_TIMEOUT)
        {
            TerminateThread(lpRequestHandlerThreads->hThreads[i], 0);
            Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%p", lpRequestHandlerThreads->hThreads[i]);
        }

        CloseHandle(lpRequestHandlerThreads->hThreads[i]);
    }
    lpRequestHandlerThreads->dwNumberOfThreads = 0;

    if (lpRequestHandlerThreads->hIocp)
        CloseHandle(lpRequestHandlerThreads->hIocp);
    lpRequestHandlerThreads->hIocp = NULL;
}

void DnsRequestHandlerPostRequest(LPDNS_REQUEST_INFO lpRequestInfo)
{
    if (!PostQueuedCompletionStatus(lpRequestInfo->lpServerInfo->RequestHandlerThreads.hIocp, 1, 1, &lpRequestInfo->Overlapped))
    {
        Error(__FUNCTION__ " - PostQueuedCompletionStatus returned FALSE, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
        DestroyDnsRequestInfo(lpRequestInfo);
    }
}

void DnsRequestHandlerProcessRequest(LPDNS_REQUEST_INFO lpRequestInfo)
{
    char addrtext[16];
    GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#ifdef __LOG_DNS_REQUESTS
    //LoggerWrite(__FUNCTION__ " - Request from %s:%d", addrtext, ntohs(lpRequestInfo->SocketAddress.sin_port));
#endif // __LOG_DNS_REQUESTS

    /*strcpy(lpRequestInfo->Buffer, "hello");
    lpRequestInfo->dwLength = 5;
    DnsPostSend(lpRequestInfo, IO_SEND);
    return;*/

    if (lpRequestInfo->dwLength < sizeof(DNS_HEADER))
    {
        Error(__FUNCTION__ " - Received data is too small for a DNS_HEADER frame (%u bytes)", lpRequestInfo->dwLength);
        DestroyDnsRequestInfo(lpRequestInfo);
        return;
    }

    LPDNS_HEADER lpDnsHeader = (LPDNS_HEADER)lpRequestInfo->Buffer;
    FlipDnsHeader(lpDnsHeader, TRUE);

#if 0
    if (DnsHeader.NumberOfAnswers ||
        DnsHeader.NumberOfAuthorities ||
        DnsHeader.NumberOfAdditional)
    {
        Error(__FUNCTION__ " - Answers/Authorities/Additional was not zero, unsupported!!");
        DestroyDnsRequestInfo(lpRequestInfo);
        return;
    }
#endif

    if (lpDnsHeader->NumberOfQuestions == 0)
    {
        Error(__FUNCTION__ " - Number of Questions is zero.");
        DestroyDnsRequestInfo(lpRequestInfo);
        return;
    }

    if (lpDnsHeader->NumberOfQuestions > 1) // This is supported by RFC but no DNS server actually include this because it's practically not going to work
    {
        Error(__FUNCTION__ " - Multiple questions in same request (Unsupported)");
        DestroyDnsRequestInfo(lpRequestInfo);
        return;
    }

#if 1
    char aLabels[128][64];
    char domainName[1024];
    DWORD dwNumLabels = 0;

    char* ptr = lpRequestInfo->Buffer + sizeof(DNS_HEADER);
    const char* end = lpRequestInfo->Buffer + lpRequestInfo->dwLength;

#define ASSERT_LEN(__l) if ((ptr + (__l)) > end) { Error(__FUNCTION__ " - Failed to read %u bytes in buffer", (__l)); DestroyDnsRequestInfo(lpRequestInfo); return; }
#define DIE() { DestroyDnsRequestInfo(lpRequestInfo); return; }

    dwNumLabels = 0;
    ASSERT_LEN(1);
    BYTE labelLen;
    while (ptr < end && (labelLen = *ptr++))
    {
        if (labelLen >= sizeof(aLabels[0]))
        {
            Error(__FUNCTION__ " - Too big label (%u bytes)", labelLen);
            DIE();
        }

        if (dwNumLabels >= 128)
        {
            Error(__FUNCTION__ " - Too many labels (128)");
            DIE();
        }

        ASSERT_LEN(labelLen);
        memcpy(aLabels[dwNumLabels], ptr, labelLen); ptr += labelLen;
        aLabels[dwNumLabels++][labelLen] = 0;
    }
    ASSERT_LEN(4);
    u_short questionType = *((u_short*)ptr); ptr += 2;
    u_short questionClass = *((u_short*)ptr); ptr += 2;

    AssembleDomainFromLabels(domainName, aLabels, dwNumLabels);
    
    // check if stupid .in-addr.arpa and .ipv6.arpa
    const char* domainNameEnd = domainName + strlen(domainName);
    BOOL bIsArpa = domainNameEnd - domainName >= 5 && strcmp(domainNameEnd - 5, ".arpa") == 0;
    if (!bIsArpa)
        g_hitLog.AddHit(domainName);

    // ptr

    DWORD dwIP;
    if (g_dnsHosts.TryResolve(domainName, &dwIP))
    {
        lpDnsHeader->NumberOfAnswers = 1;
        lpDnsHeader->Flags = 0; // reset all flags <-------------- this flags shit is weird

        lpDnsHeader->Flags = 1 << 15; // QR 1 for response

        FlipDnsHeader(lpDnsHeader, FALSE);

        *(u_short*)ptr = htons(12 | (3 << 14)); ptr += 2; // name pointer offset where bits 2 through 15 represents the actual offset value
        //char data[] = { 5, 'n', 'p', 'r', 'o', 'g', 3, 'c', 'o', 'm', 0 };
        //memcpy(ptr, data, sizeof(data)); ptr += sizeof(data);
        *(u_short*)ptr = htons(1); ptr += 2; // type
        *(u_short*)ptr = htons(1); ptr += 2; // class
        *(u_int*)ptr = htonl(30); ptr += 4; // time to live (seconds) // 3600-1h
        *(u_short*)ptr = htons(4); ptr += 2; // data length
        //*(u_int*)ptr = inet_addr("127.0.0.1"); ptr += 4;
        *(u_int*)ptr = dwIP; ptr += 4;

        lpRequestInfo->dwLength = (DWORD)(ptr - lpRequestInfo->Buffer);

#ifdef _DEBUG
        WriteBinaryPacket("OUT-INTERCEPTED-RESPONSE", lpRequestInfo->Buffer, lpRequestInfo->dwLength);
#endif // _DEBUG

        Error(__FUNCTION__ " - Resolve domain name: '%s' => %d.%d.%d.%d",
            domainName,
            BYTE(dwIP & 0xff),
            BYTE((dwIP >> 8) & 0xff),
            BYTE((dwIP >> 16) & 0xff),
            BYTE((dwIP >> 24) & 0xff));

        LPDNS_STATISTICS lpUpdateStats = g_dnsStatistics.BeginUpdate();
        ++lpUpdateStats->Resolved;
        ++lpUpdateStats->CacheHits;
        g_dnsStatistics.EndUpdate();

        DnsServerPostSend(lpRequestInfo, IO_SEND);
        return;
    }
#endif

    if (!bIsArpa)
    {
        Error("%p resolve %s", lpRequestInfo, domainName);
    }

    FlipDnsHeader(lpDnsHeader, FALSE);
    ResolveDns(lpRequestInfo);
}

DWORD WINAPI DnsRequestHandlerThread(LPVOID lp)
{
    LPDNS_SERVER_INFO lpServerInfo = (LPDNS_SERVER_INFO)lp;
    LPIO_THREADS_INFO lpRequestHandlerThreads = &lpServerInfo->RequestHandlerThreads;

    LPDNS_REQUEST_INFO lpRequestInfo;
    DWORD dwBytesTransferred;
    ULONG_PTR ulCompletionKey;

    for (;;)
    {
        if (!GetQueuedCompletionStatus(lpRequestHandlerThreads->hIocp, &dwBytesTransferred, &ulCompletionKey, (LPOVERLAPPED*)&lpRequestInfo, INFINITE))
        {
            Error(__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
            Sleep(50);
            continue;
        }

        if (dwBytesTransferred == 0) // close thread request
            break;

        DnsRequestHandlerProcessRequest(lpRequestInfo);
    }

    return 0;
}