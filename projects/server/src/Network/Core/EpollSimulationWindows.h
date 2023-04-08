#pragma once

// Provide an epoll implementation for Windows so that main code can be targeted directly for Linux.

#ifdef _WIN32
/* Flags to be passed to epoll_create2.  */
enum
{
    EPOLL_CLOEXEC = 02000000,
#define EPOLL_CLOEXEC EPOLL_CLOEXEC
    EPOLL_NONBLOCK = 04000
#define EPOLL_NONBLOCK EPOLL_NONBLOCK
};

enum EPOLL_EVENTS
{
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
    EPOLLHUP = 0x010,
#define EPOLLHUP EPOLLHUP
    EPOLLRDHUP = 0x2000,
#define EPOLLRDHUP EPOLLRDHUP
    EPOLLONESHOT = (1 << 30),
#define EPOLLONESHOT EPOLLONESHOT
    EPOLLET = (1 << 31)
#define EPOLLET EPOLLET
};

#define EPOLL_CTL_ADD 1        /* Add a file decriptor to the interface.  */
#define EPOLL_CTL_DEL 2        /* Remove a file decriptor from the interface.  */
#define EPOLL_CTL_MOD 3        /* Change file decriptor epoll_event structure.  */

typedef union epoll_data
{
    void* ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

#pragma pack(push, 1)
struct epoll_event
{
    uint32_t events;        /* Epoll events */
    epoll_data_t data;        /* User data variable */
};
#pragma pack(pop)

class EpollSocketHandler
{
    struct EpollSocketAndEvent
    {
        SOCKET Socket;
        epoll_event Event;
    };

public:
    EpollSocketHandler();
    ~EpollSocketHandler();

    int Add(SOCKET fd, const epoll_event* event);
    int Remove(SOCKET fd);
    int Update(SOCKET fd, const epoll_event* event);
    int Wait(epoll_event* events, int maxevents, int timeout);

private:
    EpollSocketAndEvent m_aSockets[128];
    size_t m_nSockets;
};

int epoll_create1(int flags);
void epoll_close(int epfd);
int epoll_ctl(int epfd, int op, SOCKET fd, epoll_event* event);
int epoll_wait(int epfd, epoll_event* events, int maxevents, int timeout);
#endif
