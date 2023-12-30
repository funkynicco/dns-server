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

#define DEFINE_COPY_MOVE_DELETE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName(ClassName&&) noexcept = delete; \
    ClassName& operator =(const ClassName&) = delete; \
    ClassName& operator =(ClassName&&) noexcept = delete

#define DEFINE_COPY_MOVE_DEFAULT(ClassName) \
    ClassName(const ClassName&) = default; \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator =(const ClassName&) = default; \
    ClassName& operator =(ClassName&&) noexcept = default

#define DEFINE_COPY_DELETE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator =(const ClassName&) = delete

#define DEFINE_COPY_DEFAULT(ClassName) \
    ClassName(const ClassName&) = default; \
    ClassName& operator =(const ClassName&) = default

#define DEFINE_MOVE_DELETE(ClassName) \
    ClassName(ClassName&&) noexcept = delete; \
    ClassName& operator =(ClassName&&) noexcept = delete

#define DEFINE_MOVE_DEFAULT(ClassName) \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator =(ClassName&&) noexcept = default

#include "PlatformDefines.h"
#include "Network/Core/EpollSimulationWindows.h"
#include "Configuration.h"
#include "Database.h"
#include "Logging/Logger.h"
#include "Globals.h"
#include "Helper.h"

char TranslateAsciiByte(char c);