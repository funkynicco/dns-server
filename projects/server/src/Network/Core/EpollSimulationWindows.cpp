#include "StdAfx.h"
#include "EpollSimulationWindows.h"

#ifdef _WIN32
static EpollSocketHandler* s_aSocketHandlers[1024] = {};
static std::mutex s_lock;

int epoll_create1(int flags)
{
    std::lock_guard l(s_lock);
    for (int i = 0; i < ARRAYSIZE(s_aSocketHandlers); i++)
    {
        if (!s_aSocketHandlers[i])
        {
            s_aSocketHandlers[i] = new EpollSocketHandler;
            return i + 1;
        }
    }

    _set_errno(EMFILE); // Too many open files
    return -1;
}

void epoll_close(int epfd)
{
    auto index = epfd - 1;
    if (index < 0 ||
        index >= ARRAYSIZE(s_aSocketHandlers))
    {
        return;
    }

    std::lock_guard l(s_lock);
    if (s_aSocketHandlers[index])
    {
        delete s_aSocketHandlers[index];
        s_aSocketHandlers[index] = nullptr;
    }
}

int epoll_ctl(int epfd, int op, SOCKET fd, epoll_event* event)
{
    auto index = epfd - 1;
    EpollSocketHandler* handler = nullptr;
    if (index < 0 ||
        index >= ARRAYSIZE(s_aSocketHandlers) ||
        !(handler = s_aSocketHandlers[index]))
    {
        _set_errno(EBADF); // Bad file number
        return -1;
    }

    switch (op)
    {
    case EPOLL_CTL_ADD:
        return handler->Add(fd, event);
    case EPOLL_CTL_DEL:
        return handler->Remove(fd);
    case EPOLL_CTL_MOD:
        return handler->Update(fd, event);
    }

    _set_errno(EINVAL); // Bad argument
    return -1;
}

int epoll_wait(int epfd, epoll_event* events, int maxevents, int timeout)
{
    const auto index = epfd - 1;
    EpollSocketHandler* handler;
    if (index < 0 ||
        index >= ARRAYSIZE(s_aSocketHandlers) ||
        !(handler = s_aSocketHandlers[index]))
    {
        _set_errno(EBADF); // Bad file number
        return -1;
    }

    return handler->Wait(events, maxevents, timeout);
}
#endif
