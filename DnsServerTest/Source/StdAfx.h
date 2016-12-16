#pragma once

#include <memory.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <Windows.h>

#include "Definitions.h"

#define SAFE_CLOSE_SOCKET(a) if ((a) != INVALID_SOCKET) { closesocket(a); (a) = INVALID_SOCKET; }
#define ASSERT(a) if(!(a)) { MessageBoxA(NULL,"Assert failed in function " __FUNCTION__ "\nin " __FILE__ "\nExpression:\n" #a,"Assert failed", MB_OK | MB_ICONWARNING);int*__a=0;*__a=0; }

#include "Console\Console.h"
#include "Logger\Logger.h"
#include "Containers\ArrayContainer.h"
#include "Containers\PtrMap.h"
#include "Containers\CircularBuffer.h"
#include "Utilities\Utilities.h"
#include "Utilities\Scanner.h"
#include "Configuration\Configuration.h"
#include "Json\Json.h"

#include "SQL\SQLClient.h"
#include "Networking\SocketPool.h"
#include "Web\WebServer.h"
#include "DNS\Hosts\DnsHosts.h"
#include "DNS\DnsServer.h"