#include "StdAfx.h"
#include "Utilities.h"

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