#pragma once

#include "Server\Server.h"

BOOL GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput);
void FlipDnsHeader(LPDNS_HEADER lpDnsHeader);
BOOL GetErrorMessageBuffer(DWORD dwError, LPSTR lpOutput, DWORD dwSizeOfOutput);
LPCSTR GetErrorMessage(DWORD dwError);