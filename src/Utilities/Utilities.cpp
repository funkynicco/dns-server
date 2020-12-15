#include "StdAfx.h"
#include "Utilities.h"

char g_internalErrorMessageBuffer[1024];
char g_internalErrorNameBuffer[256];

BOOL GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput)
{
	if (dwSizeOfOutput < 16)
		return FALSE;

	DWORD dwAddr = htonl(lpAddr->sin_addr.s_addr);

	sprintf(
		lpOutput,
		"%d.%d.%d.%d",
		(dwAddr >> 24) & 0xff,
		(dwAddr >> 16) & 0xff,
		(dwAddr >> 8) & 0xff,
		dwAddr & 0xff);

	return TRUE;
}

void MakeSocketAddress(LPSOCKADDR_IN lpAddr, const char* address, u_short port)
{
	ZeroMemory(lpAddr, sizeof(SOCKADDR_IN));
	lpAddr->sin_family = AF_INET;
	lpAddr->sin_addr.s_addr = inet_addr(address);
	lpAddr->sin_port = htons(port);
}

#ifdef __DNS_SERVER_TEST
void FlipDnsHeader(LPDNS_HEADER lpDnsHeader, BOOL NetToHost)
{
	if (NetToHost)
	{
		lpDnsHeader->TransactionID = ntohs(lpDnsHeader->TransactionID);
		lpDnsHeader->Flags = ntohs(lpDnsHeader->Flags);
		lpDnsHeader->NumberOfQuestions = ntohs(lpDnsHeader->NumberOfQuestions);
		lpDnsHeader->NumberOfAnswers = ntohs(lpDnsHeader->NumberOfAnswers);
		lpDnsHeader->NumberOfAuthorities = ntohs(lpDnsHeader->NumberOfAuthorities);
		lpDnsHeader->NumberOfAdditional = ntohs(lpDnsHeader->NumberOfAdditional);
	}
	else
	{
		lpDnsHeader->TransactionID = htons(lpDnsHeader->TransactionID);
		lpDnsHeader->Flags = htons(lpDnsHeader->Flags);
		lpDnsHeader->NumberOfQuestions = htons(lpDnsHeader->NumberOfQuestions);
		lpDnsHeader->NumberOfAnswers = htons(lpDnsHeader->NumberOfAnswers);
		lpDnsHeader->NumberOfAuthorities = htons(lpDnsHeader->NumberOfAuthorities);
		lpDnsHeader->NumberOfAdditional = htons(lpDnsHeader->NumberOfAdditional);
	}
}
#endif // __DNS_SERVER_TEST

BOOL GetErrorMessageBuffer(DWORD dwError, LPSTR lpOutput, DWORD dwSizeOfOutput)
{
	DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK;

	DWORD dwResult = FormatMessageA(
		dwFlags,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		lpOutput,
		dwSizeOfOutput,
		NULL);

	if (dwResult == 0)
		return FALSE;

	const char* begin = lpOutput;
	char* end = (lpOutput + dwResult) - 1;

	while (end > begin)
	{
		if (*end == '\r' ||
			*end == '\n' ||
			*end == ' ' ||
			*end == '\t')
			*end-- = 0;
		else
			break;
	}

	return TRUE;
}

LPCSTR GetErrorMessage(DWORD dwError)
{
	char* ptr = g_internalErrorMessageBuffer;
	const char* end = g_internalErrorMessageBuffer + sizeof(g_internalErrorMessageBuffer);

	LPCSTR lpErrorName = GetErrorName(dwError);
	if (lpErrorName)
		ptr += sprintf(ptr, "%s - ", lpErrorName);
	else
		ptr += sprintf(ptr, "[E_CODE:%u] - ", dwError);

	if (!GetErrorMessageBuffer(dwError, ptr, (DWORD)(end - ptr)))
		return "[Unknown error code]";

	return g_internalErrorMessageBuffer;
}

LPCSTR GetErrorName(DWORD dwError)
{
	//g_internalErrorNameBuffer
	if (!GetErrorDescription(dwError, g_internalErrorNameBuffer, NULL))
		return NULL;

	/*switch (dwError)
	{
	case ERROR_CONNECTION_ABORTED: return "ERROR_CONNECTION_ABORTED";
	case ERROR_NETNAME_DELETED: return "ERROR_NETNAME_DELETED";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case ERROR_OPERATION_ABORTED: return "ERROR_OPERATION_ABORTED";
	}*/

	return g_internalErrorNameBuffer;
}

#ifdef __DNS_SERVER_TEST
void AssembleDomainFromLabels(LPSTR lpOutput, char aLabels[128][64], DWORD dwNumLabels)
{
	for (DWORD i = 0; i < dwNumLabels; ++i)
	{
		if (i > 0)
			*lpOutput++ = '.';

		size_t len = strlen(aLabels[i]);
		memcpy(lpOutput, aLabels[i], len); lpOutput += len;
	}

	*lpOutput = 0;
}
#endif // __DNS_SERVER_TEST

LPCSTR GetIOMode(int IOMode)
{
	switch (IOMode)
	{
	case IO_RECV: return "IO_RECV";
	case IO_SEND: return "IO_SEND";
	case IO_RELAY_RECV: return "IO_RELAY_RECV";
	case IO_RELAY_SEND: return "IO_RELAY_SEND";
	case IO_ACCEPT: return "IO_ACCEPT";
	}

	return "E_UNKNOWN_IO_MODE";
}

DWORD CriticalSectionIncrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue)
{
	DWORD dwResult;

	EnterCriticalSection(lpCriticalSection);
	dwResult = ++(*lpdwValue);
	LeaveCriticalSection(lpCriticalSection);

	return dwResult;
}

DWORD CriticalSectionDecrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue)
{
	DWORD dwResult;

	EnterCriticalSection(lpCriticalSection);
	dwResult = --(*lpdwValue);
	LeaveCriticalSection(lpCriticalSection);

	return dwResult;
}

void GetFrequencyCounterResult(LPSTR lpOutput, LARGE_INTEGER liFrequency, LARGE_INTEGER liStart, LARGE_INTEGER liEnd)
{
	double value = (double)(liEnd.QuadPart - liStart.QuadPart) / liFrequency.QuadPart;
	if (value <= 0)
	{
		strcpy(lpOutput, "0 nanoseconds");
		return;
	}

	int n = 0;
	while (value < 1.0)
	{
		value *= 1000.0;
		++n;
	}

	switch (n)
	{
	case 0:
		if (value >= 60.0)
		{
			if (value >= 3600.0)
				sprintf(lpOutput, "%.3f hours", value / 3600.0);
			else
				sprintf(lpOutput, "%.3f minutes", value / 60.0);
		}
		else
			sprintf(lpOutput, "%.3f seconds", value);
		break;
	case 1: sprintf(lpOutput, "%.3f milliseconds", value); break;
	case 2: sprintf(lpOutput, "%.3f microseconds", value); break;
	case 3: sprintf(lpOutput, "%.0f nanoseconds", value); break;
	default:
		sprintf(lpOutput, "ERR n:%d", n);
		break;
	}
}

void DecodeAddressPort(const char* value, char* address, int* port)
{
	const char* ptr = strchr(value, ':');
	if (ptr)
	{
		memcpy(address, value, ptr - value);
		address[ptr - value] = 0;

		++ptr;
		*port = atoi(ptr);
	}
	else
	{
		strcpy(address, value);
		*port = 0;
	}
}

void WriteBinaryPacket(const char* title, const void* data, DWORD dwDataLength)
{
	HANDLE hFile;
	int maxTries = 200;
	do
	{
		hFile = CreateFile(L"dns_packet.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			if (--maxTries <= 0)
			{
				Error("Could not open dns_packet.txt, code: %u", GetLastError());
				return;
			}

			Sleep(25);
		}
	}
	while (hFile == INVALID_HANDLE_VALUE);

	DWORD dw;

	if (GetFileSize(hFile, NULL) > 0)
	{
		SetFilePointer(hFile, 0, 0, FILE_END);
		ASSERT(WriteFile(hFile, "\r\n\r\n", 4, &dw, NULL) && dw == 4);
	}

	SYSTEMTIME st;
	GetLocalTime(&st);

	char tempbuf[1024];
	int templen = sprintf(
		tempbuf,
		"[%04d/%02d/%02d %02d:%02d:%02d][%u bytes] %s\r\n",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		dwDataLength,
		title);

	ASSERT(WriteFile(hFile, tempbuf, templen, &dw, NULL) && dw == templen);

	ASSERT(WriteFile(hFile, "-----------------------------------------------\r\n", 49, &dw, NULL) && dw == 49);

	templen = 0;
	for (DWORD i = 0; i < dwDataLength; ++i)
	{
		if (i > 0)
		{
			if (i % 16 == 0)
			{
				tempbuf[templen++] = '\r';
				tempbuf[templen++] = '\n';
			}
			else
				tempbuf[templen++] = ' ';
		}

		templen += sprintf(tempbuf + templen, "%02x", ((unsigned char*)data)[i]);

		if (templen + 4 >= sizeof(tempbuf) ||
			i + 1 == dwDataLength)
		{
			// flush to file
			ASSERT(WriteFile(hFile, tempbuf, templen, &dw, NULL) && dw == templen);
			templen = 0;
		}
	}

	ASSERT(WriteFile(hFile, "\r\n-----------------------------------------------", 49, &dw, NULL) && dw == 49);

	CloseHandle(hFile);
}