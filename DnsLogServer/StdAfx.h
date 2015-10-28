#pragma once

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <iostream>
#include <Windows.h>

#include <conio.h>

#define SAFE_CLOSE_SOCKET(a) if ((a) != INVALID_SOCKET) { closesocket(a); (a) = INVALID_SOCKET; }
#define ASSERT(a) if(!(a)) { MessageBoxA(NULL,"Assert failed in function " __FUNCTION__ "\nin " __FILE__ "\nExpression:\n" #a,"Assert failed", MB_OK | MB_ICONWARNING);int*__a=0;*__a=0; }

#include "LogServer.h"