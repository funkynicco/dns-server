#include "StdAfx.h"
#include "Network/UdpServer.h"
#include "Network/WindowsWsaBootstrap.h"

#ifdef _WIN32
#define ThreadSleep(ms) Sleep(ms)
#else
#define ThreadSleep(ms) usleep(ms * 1000)
#endif

/*
 * cluster system, use udp
 * each connect to eachother using udp, nobody is "master"
 * sync dns lists
 */

static bool s_shutdown = false;

class MyUdpServer : public UdpServer
{
public:
    virtual void HandlePacket(sockaddr_in from, const char* data, size_t len) override;
};

char TranslateAsciiByte(char c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return c;

    switch (c)
    {
    case '.':
    case ' ':
    case '?':
    case '@':
    case '!':
    case '_':
    case '-':
    case '(':
    case ')':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '=':
        return c;
    }

    return '.';
}

void MyUdpServer::HandlePacket(sockaddr_in from, const char* data, size_t len)
{
    printf(
        "HandlePacket (%d.%d.%d.%d:%d):\n",
        (unsigned char)(from.sin_addr.s_addr & 0xff),
        (unsigned char)(from.sin_addr.s_addr >> 8 & 0xff),
        (unsigned char)(from.sin_addr.s_addr >> 16 & 0xff),
        (unsigned char)(from.sin_addr.s_addr >> 24 & 0xff),
        (int)htons(from.sin_port));

    for (size_t i = 0; i < len; i++)
    {
        if (i != 0 &&
            i % 16 == 0)
        {
            printf("\n");
        }

        printf("%c", TranslateAsciiByte(data[i]));
    }
    printf("\n");

    sockaddr_in fw;
    fw.sin_family = AF_INET;
    fw.sin_addr.s_addr = inet_addr("10.0.13.3"); // dns server on VM-net
    fw.sin_port = htons(53);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sendto(s, data, (int)len, 0, (const sockaddr*)&fw, sizeof(fw));
    
    char* buffer = (char*)malloc(65535);
    socklen_t addr_len = sizeof(fw);
    int received = recvfrom(s, buffer, 65535, 0, (sockaddr*)&fw, &addr_len);

    closesocket(s);

    SendTo(from, buffer, received);
    free(buffer);
}

void HandleCtrlC(int s)
{
    //exit(0);
    s_shutdown = true;
}

void InstallCtrlCHandler()
{
#ifdef _WIN32
    // ...
#else
    struct sigaction handler;
    handler.sa_handler = HandleCtrlC;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;

    sigaction(SIGINT, &handler, nullptr);
#endif
}

int main(int argc, char** argv)
{
    InstallCtrlCHandler();
    printf("Welcome.\n");

#ifdef _WIN32
    WsaBootstrap wsa;
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(53);
#else
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(6466);
#endif

    MyUdpServer server;
    server.Start(addr);

    while (!s_shutdown)
    {
        Sleep(500);
    }

    printf("Shutting down ...\n");

    server.Close();

    printf("Goodbye.\n");
    return 0;
}