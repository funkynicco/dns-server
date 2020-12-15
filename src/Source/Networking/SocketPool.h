#pragma once

enum
{
	SOCKETTYPE_UDP,
	SOCKETTYPE_TCP,
	SOCKETTYPE_MAX
};

// Safely destroys a socket from the socket pool
#define SOCKETPOOL_SAFE_DESTROY(_Type, _Socket) if ((_Socket) != INVALID_SOCKET) { SocketPoolDestroySocket(_Type, _Socket); (_Socket) = INVALID_SOCKET; }
void   InitializeSocketPool();
void   DestroySocketPool();
SOCKET SocketPoolAllocateSocket(int socketType);
void   SocketPoolDestroySocket(int socketType, SOCKET Socket);
void   SocketPoolFill();