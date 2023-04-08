#pragma once

namespace network::web
{
    class WebServerClient
    {
    public:
        ~WebServerClient();

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
        WebServer(const Configuration& configuration, ILogger* logger);
        ~WebServer();

        WebServer(const WebServer&) = delete;
        WebServer(WebServer&&) = delete;
        WebServer& operator =(const WebServer&) = delete;
        WebServer& operator =(WebServer&&) = delete;
        
        void Close();
        void Start(sockaddr_in bind_address);

        void Process();

    private:
        const Configuration& m_configuration;
        ILogger* m_logger;

        SOCKET m_socket;
        int m_epollfd;
        std::unordered_map<SOCKET, WebServerClient*> m_clients;
    };
}
