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

        operator bool() const { return m_result == 0; }

        int GetResult() const { return m_result; }
        WSADATA GetData() const { return m_wd; }

    private:
        WSADATA m_wd;
        int m_result;
    };
}
#endif