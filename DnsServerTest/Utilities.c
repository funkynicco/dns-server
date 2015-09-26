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