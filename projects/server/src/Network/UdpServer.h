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

        DEFINE_COPY_MOVE_DELETE(UdpServer);

        void Close();
        void Start(sockaddr_in bind_address, bool accept_broadcasts = false);
        void SendTo(sockaddr_in to, const char* data, size_t len) const;

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
