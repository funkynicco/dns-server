#pragma once

/*
 * This handles packets in multithreaded way!
 */
namespace network
{
    class UdpServer
    {
    public:
        UdpServer();
        virtual ~UdpServer();

        UdpServer(const UdpServer&) = delete;
        UdpServer(UdpServer&&) = delete;
        UdpServer& operator =(const UdpServer&) = delete;
        UdpServer& operator =(UdpServer&&) = delete;

        void Close();
        void Start(sockaddr_in bind_address, bool accept_broadcasts = false);
        void SendTo(sockaddr_in to, const char* data, size_t len);

    protected:
        virtual void HandlePacket(sockaddr_in from, const char* data, size_t len) {}

    private:
        SOCKET m_socket;
        std::thread m_thread;
        std::atomic<bool> m_shutdown;

        void WorkerThread();
        static void StaticWorkerThread(UdpServer* server);
    };
}
