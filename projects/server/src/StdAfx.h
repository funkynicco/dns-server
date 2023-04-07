#pragma once

#include <iostream>
#include <stdint.h>
#include <thread>
#include <atomic>
#include <regex>
#include <exception>
#include <unordered_map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <queue>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#endif

#ifndef _DEBUG
#define _DEBUG
#endif

#include <NativeLib/Exceptions.h>
#include <NativeLib/String.h>

#include "PlatformDefines.h"
#include "Network/Core/EpollSimulationWindows.h"
#include "Configuration.h"
#include "Logging/Logger.h"

char TranslateAsciiByte(char c);