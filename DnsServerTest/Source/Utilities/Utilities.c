#include "StdAfx.h"
#include "Utilities.h"

char g_internalErrorMessageBuffer[1024];

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

void FlipDnsHeader(LPDNS_HEADER lpDnsHeader)
{
	lpDnsHeader->TransactionID = ntohs(lpDnsHeader->TransactionID);
	lpDnsHeader->Flags = ntohs(lpDnsHeader->Flags);
	lpDnsHeader->NumberOfQuestions = ntohs(lpDnsHeader->NumberOfQuestions);
	lpDnsHeader->NumberOfAnswers = ntohs(lpDnsHeader->NumberOfAnswers);
	lpDnsHeader->NumberOfAuthorities = ntohs(lpDnsHeader->NumberOfAuthorities);
	lpDnsHeader->NumberOfAdditional = ntohs(lpDnsHeader->NumberOfAdditional);
}

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
	if (!GetErrorMessageBuffer(dwError, g_internalErrorMessageBuffer, sizeof(g_internalErrorMessageBuffer)))
		return "[Unknown error code]";

	return g_internalErrorMessageBuffer;
}

void AssembleDomainFromLabels(LPSTR lpOutput, char aLabels[16][64], DWORD dwNumLabels)
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

LPCSTR GetIOMode(int IOMode)
{
	switch (IOMode)
	{
	case IO_RECV: return "IO_RECV";
	case IO_SEND: return "IO_SEND";
	case IO_RELAY_RECV: return "IO_RELAY_RECV";
	case IO_RELAY_SEND: return "IO_RELAY_SEND";
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
	char* ptr = strchr(value, ':');
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