#include "StdAfx.h"
#include "WebServer.h"

#include "Network/NetServerException.h"

namespace network::web
{
    WebServerClient::WebServerClient(SOCKET s, sockaddr_in addr) :
        m_socket(s),
        m_addr(addr)
    {
    }

    WebServerClient::~WebServerClient()
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
        }
    }

    void WebServerClient::OnDataReceived(const char* buffer, size_t len)
    {
        char response[4096];
        sprintf_s(response, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: 11\n\nhello world");
        send(m_socket, response, (int)strlen(response), 0);
    }
}