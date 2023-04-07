#pragma once

#ifndef _WIN32 // Define things for Linux here

/*
 * Represents a socket identifier.
 */
#define SOCKET int

/*
 * Represents an invalid socket.
 */
#define INVALID_SOCKET ((int)-1)

/*
 * Represents a socket error.
 * Socket related methods may return this value, such as bind, listen, recv, send, etc.
 */
#define SOCKET_ERROR ((int)-1)

/*
 * Provides a millisecond sleep interface for Linux that uses usleep.
 */
#define Sleep( __ms ) usleep( __ms * 1000 )
#define VK_ESCAPE 0x1B
#define closesocket( __sock ) close( __sock )
#define epoll_close(fd) close(fd)

#define ERR_NR errno

//#define size_t uint32_t

#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

#else

#define ERR_NR WSAGetLastError()
typedef int socklen_t;

#endif