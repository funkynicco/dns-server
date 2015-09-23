#pragma once

#include "Server\Server.h"

BOOL GetIPFromSocketAddress(LPSOCKADDR_IN lpAddr, LPSTR lpOutput, DWORD dwSizeOfOutput);
void FlipDnsHeader(LPDNS_HEADER lpDnsHeader);