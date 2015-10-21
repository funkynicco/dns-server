#pragma once

#include "DNS\DnsServer.h"

#define GetRealMemoryPointer(__obj) ((char*)(__obj) - 1)
#define IsDestroyed(__obj) (*((char*)(__obj) - 1) != 0)
#define SetDestroyed(__obj) *((char*)(__obj) - 1) = 1;
#define ClearDestroyed(__obj) *((char*)(__obj) - 1) = 0;

BOOL			GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput);
void			MakeSocketAddress(LPSOCKADDR_IN lpAddr, const char* address, u_short port);
void			FlipDnsHeader(LPDNS_HEADER lpDnsHeader);
BOOL			GetErrorMessageBuffer(DWORD dwError, LPSTR lpOutput, DWORD dwSizeOfOutput);
LPCSTR			GetErrorMessage(DWORD dwError);
void			AssembleDomainFromLabels(LPSTR lpOutput, char aLabels[16][64], DWORD dwNumLabels);
LPCSTR			GetIOMode(int IOMode);
DWORD			CriticalSectionIncrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);
DWORD			CriticalSectionDecrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);
void			GetFrequencyCounterResult(LPSTR lpOutput, LARGE_INTEGER liFrequency, LARGE_INTEGER liStart, LARGE_INTEGER liEnd);
void			DecodeAddressPort(const char* value, char* address, int* port);