#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifdef __DNS_SERVER_TEST
#include "DNS\DnsServer.h"
#else // __DNS_SERVER_TEST
#define Error(_Sz, ...) printf(__FUNCTION__ " - "_Sz, __VA_ARGS__)
#endif // __DNS_SERVER_TEST

#define GetRealMemoryPointer(__obj) ((char*)(__obj) - 1)
#define IsDestroyed(__obj) (*((char*)(__obj) - 1) != 0)
#define SetDestroyed(__obj) *((char*)(__obj) - 1) = 1;
#define ClearDestroyed(__obj) *((char*)(__obj) - 1) = 0;

BOOL			GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput);
void			MakeSocketAddress(LPSOCKADDR_IN lpAddr, const char* address, u_short port);
#ifdef __DNS_SERVER_TEST
void			FlipDnsHeader(LPDNS_HEADER lpDnsHeader, BOOL NetToHost);
#endif // __DNS_SERVER_TEST
void			InitializeErrorDescriptionTable();
BOOL			GetErrorMessageBuffer(DWORD dwError, LPSTR lpOutput, DWORD dwSizeOfOutput);
LPCSTR			GetErrorMessage(DWORD dwError);
LPCSTR			GetErrorName(DWORD dwError);
BOOL			GetErrorDescription(DWORD dwError, LPSTR lpName, LPSTR lpDescription);
#ifdef __DNS_SERVER_TEST
void			AssembleDomainFromLabels(LPSTR lpOutput, char aLabels[128][64], DWORD dwNumLabels);
#endif // __DNS_SERVER_TEST
LPCSTR			GetIOMode(int IOMode);
DWORD			CriticalSectionIncrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);
DWORD			CriticalSectionDecrementValue(LPCRITICAL_SECTION lpCriticalSection, LPDWORD lpdwValue);
void			GetFrequencyCounterResult(LPSTR lpOutput, LARGE_INTEGER liFrequency, LARGE_INTEGER liStart, LARGE_INTEGER liEnd);
void			DecodeAddressPort(const char* value, char* address, int* port);

void			WriteBinaryPacket(const char* title, const void* data, DWORD dwDataLength);

#ifdef __cplusplus
}
#endif // __cplusplus