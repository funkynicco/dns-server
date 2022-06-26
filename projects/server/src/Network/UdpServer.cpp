#include "StdAfx.h"
#include "UdpServer.h"

UdpServer::UdpServer() :
    m_socket(INVALID_SOCKET)
{
}

UdpServer::~UdpServer()
{
    Close();
}

void UdpServer::Close()
{
    if (m_socket == INVALID_SOCKET)
    {
        return;
    }

    m_shutdown.store(true);
    m_thread.join();

    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
}

void UdpServer::Start(sockaddr_in bind_address)
{
    if (m_socket != INVALID_SOCKET)
    {
        throw UdpServerException("Server already started");
    }

    m_shutdown.store(false);

    auto s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
    {
        throw UdpServerException("Failed to create socket, code: {err_nr}", ERR_NR);
    }

    if (bind(s, (const sockaddr*)&bind_address, sizeof(bind_address)) == SOCKET_ERROR)
    {
        int err_nr = ERR_NR;
        closesocket(s);
        throw UdpServerException("Failed to bind socket to address, code: {err_nr}", err_nr);
    }

    m_socket = s;
    m_thread = std::thread(StaticWorkerThread, this);
}

void UdpServer::SendTo(sockaddr_in to, const char* data, size_t len)
{
    if (m_socket == INVALID_SOCKET)
    {
        return;
    }

    if (sendto(m_socket, data, (int)len, 0, (const sockaddr*)&to, sizeof(to)) == SOCKET_ERROR)
    {
        throw UdpServerException("Failed to send to address, code: {err_nr}", ERR_NR);
    }
}

void UdpServer::WorkerThread()
{
    char* buffer = (char*)malloc(4096); // includes packet size and sizeof(sockaddr_in)

    struct timeval tv = {};
    tv.tv_usec = 250000;

    fd_set fd;

    while (!m_shutdown.load())
    {
        FD_ZERO(&fd);
        FD_SET(m_socket, &fd);

        if (select(m_socket + 1, &fd, nullptr, nullptr, &tv) > 0 &&
            FD_ISSET(m_socket, &fd))
        {
            sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            int received = recvfrom(
                m_socket,
                buffer + sizeof(sockaddr_in) + sizeof(int),
                4096 - sizeof(sockaddr_in) - sizeof(int),
                0, /*flags*/
                (sockaddr*)&addr,
                &addr_len);

            if (received > 0)
            {
                HandlePacket(addr, buffer + sizeof(sockaddr_in) + sizeof(int), received);
            }
        }
    }

    free(buffer);
}

void UdpServer::StaticWorkerThread(UdpServer* server)
{
    server->WorkerThread();
}