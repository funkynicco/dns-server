#pragma once

#include <iostream>
#include <stdint.h>
#include <thread>
#include <atomic>

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


#include "PlatformDefines.h"