#include "StdAfx.h"
#include "WebServer.h"

#include "Network/NetServerException.h"

namespace network::web
{
    WebServer::WebServer(const Configuration& configuration, ILogger* logger) :
        m_configuration(configuration),
        m_logger(logger),
        m_socket(INVALID_SOCKET),
        m_epollfd(0)
    {
    }

    WebServer::~WebServer()
    {
        Close();
    }

    void WebServer::Close()
    {
        if (m_socket == INVALID_SOCKET)
        {
            return;
        }

        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        
        // TODO: kill clients...

        epoll_close(m_epollfd);
        m_epollfd = 0;
    }
    
    void WebServer::Start(sockaddr_in bind_address)
    {
        if (m_socket != INVALID_SOCKET)
        {
            throw NetServerException("Server already started");
        }

        int epfd = epoll_create1(0);
        if (!epfd)
        {
            throw NetServerException("Failed to create epoll instance");
        }

        auto s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET)
        {
            epoll_close(epfd);
            throw NetServerException("Failed to create socket, code: {err_nr}", ERR_NR);
        }

        if (bind(s, (const sockaddr*)&bind_address, sizeof(bind_address)) == SOCKET_ERROR)
        {
            int err_nr = ERR_NR;
            epoll_close(epfd);
            closesocket(s);
            throw NetServerException("Failed to bind socket to address, code: {err_nr}", err_nr);
        }

        if (listen(s, SOMAXCONN) == SOCKET_ERROR)
        {
            int err_nr = ERR_NR;
            epoll_close(epfd);
            closesocket(s);
            throw NetServerException("Failed to listen on socket, code: {err_nr}", err_nr);
        }

        epoll_event ev = {};
        ev.events = EPOLLIN;
        ev.data.u64 = m_socket;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev) != 0)
        {
            int err_nr = ERR_NR;
            epoll_close(epfd);
            closesocket(s);
            throw NetServerException("Failed add socket to epoll instance, code: {err_nr}", err_nr);
        }

        m_socket = s;
        m_epollfd = epfd;
    }

    void WebServer::Process()
    {
        epoll_event events[16];
        int num = epoll_wait(m_epollfd, events, ARRAYSIZE(events), 1);
        for (int i = 0; i < num; i++)
        {
            epoll_event* ev = &events[i];
            if (ev->data.u64 == m_socket)
            {
                // listen socket
                sockaddr_in addr;
                socklen_t addr_len = sizeof(addr);
                SOCKET c = accept(m_socket, (sockaddr*)&addr, &addr_len);
                if (c != INVALID_SOCKET)
                {
                    auto client = new WebServerClient(c, addr);

                    epoll_event ev = {};
                    ev.data.u64 = c;
                    ev.events = EPOLLIN;
                    epoll_ctl(m_epollfd, EPOLL_CTL_ADD, client->GetSocket(), &ev); // TODO: check return value

                    m_clients.insert(std::make_pair(client->GetSocket(), client));
                }
            }
            else
            {
                static char buffer[65536];

                auto it = m_clients.find(ev->data.u64);
                if (it != m_clients.end())
                {
                    auto client = it->second;

                    int read = recv(client->GetSocket(), buffer, sizeof(buffer), 0);
                    if (read <= 0)
                    {
                        delete client;
                        m_clients.erase(it);
                        continue;
                    }

                    client->OnDataReceived(buffer, (size_t)read);
                }
            }
        }
    }
}
