#pragma once

namespace network::web
{
    class WebServerClient
    {
    public:
        ~WebServerClient();

        DEFINE_COPY_MOVE_DELETE(WebServerClient);

        [[nodiscard]] SOCKET GetSocket() const { return m_socket; }
        [[nodiscard]] sockaddr_in GetAddress() const { return m_addr; }

    private:
        friend class WebServer;
        WebServerClient(SOCKET s, sockaddr_in addr);
        void OnDataReceived(const char* buffer, size_t len);

        SOCKET m_socket;
        sockaddr_in m_addr;
    };

    class WebServer
    {
    public:
        WebServer();
        ~WebServer();

        DEFINE_COPY_MOVE_DELETE(WebServer);
        
        void Close();
        void Start(sockaddr_in bind_address);

        void Process();

    private:
        SOCKET m_socket;
        int m_epollfd;
        std::unordered_map<SOCKET, WebServerClient*> m_clients;
    };
}
