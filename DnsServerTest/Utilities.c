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