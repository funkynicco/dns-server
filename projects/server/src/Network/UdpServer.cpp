#include "StdAfx.h"
#include "UdpServer.h"

#include "NetServerException.h"

namespace network
{
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

    void UdpServer::Start(const sockaddr_in bind_address, const bool accept_broadcasts)
    {
        if (m_socket != INVALID_SOCKET)
        {
            throw NetServerException("Server already started");
        }

        m_shutdown.store(false);

        auto s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET)
        {
            throw NetServerException("Failed to create socket, code: {err_nr}", ERR_NR);
        }

        if (bind(s, (const sockaddr*)&bind_address, sizeof(bind_address)) == SOCKET_ERROR)
        {
            const int err_nr = ERR_NR;
            closesocket(s);
            throw NetServerException("Failed to bind socket to address, code: {err_nr}", err_nr);
        }

        if (accept_broadcasts)
        {
            constexpr int enabled = 1;
            if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&enabled, sizeof(enabled)) != 0)
            {
                const int err_nr = ERR_NR;
                closesocket(s);
                throw NetServerException("Failed to enable broadcast mode on socket, code: {err_nr}", err_nr);
            }
        }

        m_socket = s;
        m_thread = std::thread(StaticWorkerThread, this);
    }

    void UdpServer::SendTo(const sockaddr_in to, const char* data, const size_t len) const
    {
        if (m_socket == INVALID_SOCKET)
        {
            return;
        }

        if (sendto(m_socket, data, (int)len, 0, (const sockaddr*)&to, sizeof(to)) <= 0)
        {
            throw NetServerException("Failed to send to address, code: {err_nr}", ERR_NR);
        }
    }

    void UdpServer::WorkerThread()
    {
        const auto buffer = (char*)malloc(4096); // includes packet size and sizeof(sockaddr_in)

        timeval tv = {};
        tv.tv_usec = 250000;

        fd_set fd;

        while (!m_shutdown.load())
        {
            FD_ZERO(&fd);
            FD_SET(m_socket, &fd);

            if (select(int(m_socket + 1), &fd, nullptr, nullptr, &tv) > 0 &&
                FD_ISSET(m_socket, &fd))
            {
                sockaddr_in addr;
                socklen_t addr_len = sizeof(addr);
                const int received = recvfrom(
                    m_socket,
                    buffer + sizeof(sockaddr_in) + sizeof(int),
                    4096 - sizeof(sockaddr_in) - sizeof(int),
                    0, /*flags*/
                    (sockaddr*)&addr,
                    &addr_len);

                if (received > 0)
                {
                    memcpy(buffer, &addr, sizeof(sockaddr_in));
                    *((int*)(buffer + sizeof(sockaddr_in))) = received;
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
}
