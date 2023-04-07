#include "StdAfx.h"
#include "DnsServer.h"

namespace network::dns
{
    DnsServer::DnsServer(const Configuration& configuration, ILogger* logger) :
        m_configuration(configuration),
        m_logger(logger)
    {
    }

    void DnsServer::HandlePacket(sockaddr_in from, const char* data, size_t len)
    {
        char ip[16];
        AddrToStr(ip, from.sin_addr);

        nl::String str;

        str += nl::String::Format(
            "DnsServer::HandlePacket (%s:%d):\n",
            ip,
            (int)htons(from.sin_port));

        for (size_t i = 0; i < len; i++)
        {
            if (i != 0 &&
                i % 16 == 0)
            {
                str.Append('\n');
            }

            str.Append(TranslateAsciiByte(data[i]));
        }

        m_logger->Log(LogType::Debug, "DnsServer", str);

#if 0
        sockaddr_in fw;
        fw.sin_family = AF_INET;
        fw.sin_addr.s_addr = inet_addr("1.2.3.4");
        fw.sin_port = htons(53);

        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sendto(s, data, (int)len, 0, (const sockaddr*)&fw, sizeof(fw));

        char* buffer = (char*)malloc(65535);
        socklen_t addr_len = sizeof(fw);
        int received = recvfrom(s, buffer, 65535, 0, (sockaddr*)&fw, &addr_len);

        closesocket(s);

        SendTo(from, buffer, received);
        free(buffer);
#endif
    }

    void DnsServer::Process()
    {
    }
}
