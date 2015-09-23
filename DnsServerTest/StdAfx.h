#pragma once

#include <memory.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <Windows.h>

#define SAFE_CLOSE_SOCKET(a) if ((a) != INVALID_SOCKET) { closesocket(a); (a) = INVALID_SOCKET; }

#include "Utilities.h"
#include "ArrayContainer.h"

#include "Server\Server.h"