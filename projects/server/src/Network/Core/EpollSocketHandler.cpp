#include "StdAfx.h"
#include "EpollSimulationWindows.h"

#ifdef _WIN32
EpollSocketHandler::EpollSocketHandler()
{
    memset(m_aSockets, 0, sizeof(m_aSockets));
    m_nSockets = 0;
}

EpollSocketHandler::~EpollSocketHandler()
{
}

int EpollSocketHandler::Add(SOCKET fd, epoll_event* event)
{
    if (event->events != EPOLLIN) // Anything but EPOLLIN is not supported
    {
        _set_errno(EINVAL);
        return -1;
    }

    for (size_t i = 0; i < m_nSockets; i++)
    {
        if (fd == m_aSockets[i].Socket)
        {
            _set_errno(EEXIST); // Already exists
            return -1;
        }
    }

    if (m_nSockets == ARRAYSIZE(m_aSockets))
    {
        _set_errno(ENOSPC); // Too many sockets
        return -1;
    }

    size_t i = m_nSockets++;
    m_aSockets[i].Socket = fd;
    memcpy(&m_aSockets[i].Event, event, sizeof(epoll_event));
    return 0;
}

int EpollSocketHandler::Remove(SOCKET fd)
{
    for (size_t i = 0; i < m_nSockets; i++)
    {
        if (fd == m_aSockets[i].Socket)
        {
            // remove and replace with the end
            memcpy(&m_aSockets[i], &m_aSockets[--m_nSockets], sizeof(EpollSocketAndEvent));
            return 0;
        }
    }

    _set_errno(ENOENT); // Not registered in this instance
    return -1;
}

int EpollSocketHandler::Update(SOCKET fd, epoll_event* event)
{
    if (event->events != EPOLLIN) // Anything but EPOLLIN is not supported
    {
        _set_errno(EINVAL);
        return -1;
    }

    for (size_t i = 0; i < m_nSockets; i++)
    {
        if (fd == m_aSockets[i].Socket)
        {
            memcpy(&m_aSockets[i].Event, event, sizeof(epoll_event));
            return 0;
        }
    }

    _set_errno(ENOENT); // Not registered in this instance
    return -1;
}

int EpollSocketHandler::Wait(epoll_event* events, int maxevents, int timeout)
{
    /* 
     * Note: The timeout is not a true time for this Wait function.
     * If there are more than FD_SETSIZE then the timeout will be used in "select" for every FD_SETSIZE block
     * 
     */

    if (maxevents <= 0)
    {
        _set_errno(EINVAL);
        return -1;
    }

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1000;

    fd_set fd;
    FD_ZERO(&fd);

    SOCKET max_socket = 0;

    int num_events = 0;
    
    size_t i = 0;
    while (i < m_nSockets)
    {
        // add to the fd set
        size_t num = min(FD_SETSIZE, m_nSockets - i);
        for (size_t j = 0; j < num; j++)
        {
            SOCKET s = m_aSockets[i + j].Socket;
            FD_SET(s, &fd);
            if (max_socket < s)
            {
                max_socket = s;
            }
        }

        // select and loop through sockets still in the fd set (ready for reading)
        if (select(0 /* ignored on windows */, &fd, nullptr, nullptr, &tv) > 0)
        {
            for (size_t j = 0; j < num; j++)
            {
                SOCKET s = m_aSockets[i + j].Socket;
                if (FD_ISSET(s, &fd))
                {
                    memcpy(&events[num_events++], &m_aSockets[i + j].Event, sizeof(epoll_event));
                    if (num_events == maxevents)
                    {
                        return num_events;
                    }
                }
            }
        }

        i += num;
    }

    return num_events;
}
#endif