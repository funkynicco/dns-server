#pragma once

#include "Server\Server.h"

BOOL	GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput);
void	FlipDnsHeader(LPDNS_HEADER lpDnsHeader);
BOOL	GetErrorMessageBuffer(DWORD dwError, LPSTR lpOutput, DWORD dwSizeOfOutput);
LPCSTR	GetErrorMessage(DWORD dwError);
void	AssembleDomainFromLabels(LPSTR lpOutput, char aLabels[16][64], DWORD dwNumLabels);
LPCSTR	GetIOMode(int IOMode);
DWORD	CriticalSectionIncrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);
DWORD	CriticalSectionDecrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);