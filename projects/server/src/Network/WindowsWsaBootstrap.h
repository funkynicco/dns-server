#pragma once

#ifdef _WIN32
namespace network
{
    class WsaBootstrap
    {
    public:
        WsaBootstrap()
        {
            m_result = WSAStartup(MAKEWORD(2, 2), &m_wd);
        }

        ~WsaBootstrap()
        {
            WSACleanup();
        }

        WsaBootstrap(const WsaBootstrap&) = delete;
        WsaBootstrap(WsaBootstrap&&) = delete;
        WsaBootstrap& operator =(const WsaBootstrap&) = delete;
        WsaBootstrap& operator =(WsaBootstrap&&) = delete;

        explicit operator bool() const { return m_result == 0; }

        [[nodiscard]] int GetResult() const { return m_result; }
        [[nodiscard]] WSADATA GetData() const { return m_wd; }

    private:
        WSADATA m_wd;
        int m_result;
    };
}
#endif