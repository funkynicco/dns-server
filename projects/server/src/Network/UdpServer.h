#pragma once

#include <exception>

/*
 * This handles packets in multithreaded way!
 */

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
    void Start(sockaddr_in bind_address);
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

class UdpServerException : public std::exception
{
public:
    UdpServerException(const char* message, int err_nr = 0)
    {
        m_what = FormatExceptionMessage(message, err_nr);
    }

    virtual const char* what() const noexcept override
    {
        return m_what.c_str();
    }

private:
    std::string m_what;

    static std::string FormatExceptionMessage(const char* message, int err_nr)
    {
        size_t len = strlen(message);

        std::string res;
        res.reserve(len);
        for (size_t i = 0; i < len;)
        {
            if (message[i] == '{' &&
                i + 8 <= len && // {err_nr}
                memcmp(message + i, "{err_nr}", 8) == 0)
            {
                i += 8;
                char str[32];
                sprintf(str, "%d", err_nr);
                res.append(str);
                continue;
            }

            res.append(1, message[i]);
            i++;
        }

        return res;
    }
};