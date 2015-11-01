#pragma once

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <MSWSock.h>

#ifdef __cplusplus
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <list>
using namespace std;
#else // __cplusplus
#include <memory.h>
#include <stdio.h>
#endif // __cplusplus

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <Windows.h>

#include <time.h>
#include <conio.h>

#define SAFE_DELETE(a) if(a) { delete (a); (a) = NULL; }
#define SAFE_CLOSE_SOCKET(a) if ((a) != INVALID_SOCKET) { closesocket(a); (a) = INVALID_SOCKET; }
#define ASSERT(a) if(!(a)) { MessageBoxA(NULL,"Assert failed in function " __FUNCTION__ "\nin " __FILE__ "\nExpression:\n" #a,"Assert failed", MB_OK | MB_ICONWARNING);int*__a=0;*__a=0; }

#include "../DnsServerTest/Source/Containers/CircularBuffer.h"
#include "../DnsServerTest/Source/Networking/NetworkDefines.h"
#include "../DnsServerTest/Source/Utilities/Utilities.h"
#include "../DnsServerTest/Source/Utilities/Scanner.h"
#include "../DnsServerTest/Source/Json/Json.h"

#ifdef __cplusplus
#include "Configuration\Configuration.h"
#include "Logger\LogCache.h"
#include "Logger\Logger.h"
#include "LogServer.h"
#include "LogWebServer\LogWebServer.h"
#endif // __cplusplus