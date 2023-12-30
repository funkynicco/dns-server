#include "StdAfx.h"
#include "ClusterServer.h"

#define BindPacket(header, func) m_packets[(int)(header)] = &ClusterServer::func

namespace network::cluster
{
    ClusterServer::ClusterServer()
    {
        const auto configuration = Globals::Get<IConfiguration>();
        const auto logger = Globals::Get<ILogger>();
        
        memset(&m_myaddr, 0, sizeof(m_myaddr));
        m_broadcastaddr = MakeAddr(
            configuration->GetClusterBroadcast().c_str(),
            configuration->GetClusterPort()
        );

        m_joined = false;
        m_tNextBroadcastJoin = 0;
        m_tTimeoutJoining = time(nullptr) + configuration->GetClusterJoinTimeout();
        m_epollfd = epoll_create1(0);
        if (m_epollfd <= 0)
        {
            logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_create1 failed with code: {}", errno));
        }

        // bind functions
        memset(m_packets, 0, sizeof(m_packets));
        BindPacket(PacketHeader::JoinRequest, OnJoinRequest);
        BindPacket(PacketHeader::JoinResponse, OnJoinResponse);
        BindPacket(PacketHeader::HandshakeRequest, OnHandshakeRequest);
    }

    ClusterServer::~ClusterServer()
    {
        for (const auto it : m_clients)
        {
            delete it.second;
        }

        epoll_close(m_epollfd);
    }

    void ClusterServer::HandlePacket(sockaddr_in from, const char* data, size_t len)
    {
        // called from UdpServer thread!

        if (from.sin_addr.s_addr == m_myaddr.s_addr)
        {
            // discard own broadcast messages
            return;
        }

        if (!m_ipv4filter.Match(from.sin_addr)) // (thread-safe) uses shared_mutex for read/write lock
        {
            // discard packets from other networks
            return;
        }

        const auto packet_data = reinterpret_cast<const ClusterPacketData*>(data);
        if (!ValidateClusterPacketData(len, packet_data))
        {
            return;
        }

        const auto packet = ClusterPacketPool::Acquire();
        packet->From = from;
        memcpy(&packet->Data, packet_data, sizeof(packet->Data));

        std::lock_guard g(m_receive_queue_lock);
        m_receive_queue.push(packet);
    }

    bool ClusterServer::ValidateClusterPacketData(size_t raw_packet_length, const ClusterPacketData* packet_data)
    {
        if (raw_packet_length < MIN_PACKET_SIZE)
        {
            // discard invalid udp packets...
            return false;
        }

        if (memcmp(packet_data->Signature, PACKET_SIGNATURE, sizeof(packet_data->Signature)) != 0)
        {
            // discard: packet signature is not valid
            return false;
        }

        if (packet_data->DataSize > raw_packet_length - MIN_PACKET_SIZE)
        {
            // discard: the packet specified more bytes than was received
            return false;
        }

        return true;
    }

    void ClusterServer::ProcessPacket(ClusterPacket* packet)
    {
        const auto logger = Globals::Get<ILogger>();
        
        char ip[16];
        AddrToStr(ip, packet->From.sin_addr);
        logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
            "[{}:{}] {} (seq: {}, size: {})",
            ip,
            ntohs(packet->From.sin_port),
            PacketHeaderToString(packet->Data.Header).c_str(),
            packet->Data.Sequence,
            packet->Data.DataSize
        ));

        // packet handler...
        int pid = (int)packet->Data.Header;
        if (pid < 0 ||
            pid >= ARRAYSIZE(m_packets))
        {
            packet->Release();
            return;
        }

        const auto handler = m_packets[pid];
        if (!handler)
        {
            packet->Release();
            return;
        }

        (this->*handler)(packet);
    }

    void ClusterServer::ProcessPacketQueue()
    {
        ClusterPacket* queue[512];
        size_t queue_size = 0;

        { // quickly copy up to 512 packets from the receiving queue
            std::lock_guard g(m_receive_queue_lock);
            while (queue_size < ARRAYSIZE(queue))
            {
                if (m_receive_queue.empty())
                {
                    break;
                }

                queue[queue_size++] = m_receive_queue.front();
                m_receive_queue.pop();
            }
        }

        for (size_t i = 0; i < queue_size; i++)
        {
            ProcessPacket(queue[i]);
        }
    }

    void ClusterServer::Process()
    {
        const auto logger = Globals::Get<ILogger>();
        
        ProcessPacketQueue();
        ReceiveAllClients();

        const auto now = time(nullptr);

        if (!m_joined)
        {
            if (m_tTimeoutJoining < now)
            {
                // ... start a new cluster
                logger->Log(LogType::Info, "ClusterServer", "Timeout joining cluster - starting new...");
                m_joined = true;
            }
            else if (m_tNextBroadcastJoin < now)
            {
                m_tNextBroadcastJoin = now + 1;

                const auto join_packet = ClusterPacketData::Initialize(PacketHeader::JoinRequest, 0);
                Broadcast(&join_packet);
            }
        }
        else
        {
            // already joined in cluster...
        }

        // process clients
        for (auto it = m_clients.begin(); it != m_clients.end();)
        {
            if (it->second->IsDelete())
            {
                if (epoll_ctl(m_epollfd, EPOLL_CTL_DEL, it->second->GetSocket(), nullptr) != 0)
                {
                    logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_ctl del failed with code: {}", errno));
                }

                logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
                    "Deleted {}:{}: {}",
                    AddrToStr(it->second->GetAddress().sin_addr).c_str(),
                    ntohs(it->second->GetAddress().sin_port),
                    it->second->GetDeleteReason().c_str()
                ));

                delete it->second;
                it = m_clients.erase(it);
                continue;
            }

            it->second->Process();
            ++it;
        }
    }

    void ClusterServer::ReceiveAllClients()
    {
        const auto logger = Globals::Get<ILogger>();
        
        epoll_event events[MAX_EPOLL_EVENTS];
        const int ready = epoll_wait(m_epollfd, events, MAX_EPOLL_EVENTS, 1);
        if (ready < 0)
        {
            logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_wait failed with code: {}", errno));
        }

        for (int i = 0; i < ready; i++)
        {
            static char buffer[4096];
            const auto ev = &events[i];
            const auto client = static_cast<ClusterServerClient*>(ev->data.ptr);
            if (client->IsDelete())
            {
                continue;
            }

            sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            const int received = recvfrom(client->GetSocket(), buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addrlen);
            if (received <= 0)
            {
                // error
                client->Delete("Network error: recvfrom failed");
                continue;
            }

            const auto packet_data = reinterpret_cast<const ClusterPacketData*>(buffer);
            if (!ValidateClusterPacketData((size_t)received, packet_data))
            {
                continue; // drop invalid packet
            }

            client->SetAddress(addr); // update remote address
            client->OnPacketReceived(packet_data);
        }
    }

    void ClusterServer::Send(const sockaddr_in to, const ClusterPacketData* packet_data)
    {
        SendTo(
            to,
            reinterpret_cast<const char*>(packet_data),
            MIN_PACKET_SIZE + packet_data->DataSize
        );
    }

    void ClusterServer::Broadcast(const ClusterPacketData* packet_data)
    {
        SendTo(
            m_broadcastaddr,
            reinterpret_cast<const char*>(packet_data),
            MIN_PACKET_SIZE + packet_data->DataSize
        );
    }

    void ClusterServer::OnJoinRequest(ClusterPacket* packet)
    {
        //log("OnJoinRequest from %s", AddrToStr(from.sin_addr).c_str());

        if (!m_joined)
        {
            // we are trying to join the cluster, don't respond to other join requests
            packet->Release();
            return;
        }

        // read potential data...


        // ... send our knowledge of cluster to the new client
        auto send = ClusterPacketData::Initialize(PacketHeader::JoinResponse, 0);
        uint8_t* buf = send.Data;

        *(uint16_t*)buf = (uint16_t)m_clients.size() + 1; // +1 for myself
        buf += sizeof(uint16_t);

        // add "myself"
        uint32_t addr = this->m_myaddr.s_addr;
        memcpy(buf, &addr, sizeof(addr));
        buf += sizeof(addr);

        for (const auto [_, client] : m_clients)
        {
            addr = client->GetAddress().sin_addr.s_addr;
            memcpy(buf, &addr, sizeof(addr));
            buf += sizeof(addr);
        }

        send.DataSize = uint16_t(buf - send.Data);
        Send(packet->From, &send);

        packet->Release();
    }

    void ClusterServer::OnJoinResponse(ClusterPacket* packet)
    {
        const auto configuration = Globals::Get<IConfiguration>();
        const auto logger = Globals::Get<ILogger>();
        
        //log("OnJoinResponse from %s", AddrToStr(from.sin_addr).c_str());

        // join information from another server in the cluster...
        // ... connect to each and sync

        m_joined = true;

        logger->Log(LogType::Debug, "ClusterServer", nl::String::Format("Received join response from {}", AddrToStr(packet->From.sin_addr).c_str()));

        const uint16_t clients = *(uint16_t*)packet->Data.Data;
        logger->Log(LogType::Debug, "ClusterServer", nl::String::Format("Clients: {}", clients));
        for (uint16_t i = 0; i < clients; i++)
        {
            uint32_t addr = *(uint32_t*)(packet->Data.Data + sizeof(uint16_t) + sizeof(uint32_t) * i);
            logger->Log(LogType::Debug, "ClusterServer", nl::String::Format("[{}]: {}", i, AddrToStr(addr).c_str()));

            if (addr == m_myaddr.s_addr)
            {
                // skip "myself"
                continue;
            }

            auto it = m_clients.find(addr);
            if (it != m_clients.end())
            {
                if (!it->second->IsDelete())
                {
                    // skip clients we already know of
                    continue;
                }

                // client is pending deletion, carry out deletion now forcefully and remove from m_clients
                if (epoll_ctl(m_epollfd, EPOLL_CTL_DEL, it->second->GetSocket(), nullptr) != 0)
                {
                    logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_ctl del failed with code: {}", errno));
                }

                logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
                    "[OnJoinResponse] Deleted {}: {}",
                    AddrToStr(it->second->GetAddress().sin_addr).c_str(),
                    it->second->GetDeleteReason().c_str()
                ));
                delete it->second;
                m_clients.erase(it);
            }

            // add new client
            sockaddr_in addr_in;
            addr_in.sin_family = AF_INET;
            addr_in.sin_addr.s_addr = addr;
            addr_in.sin_port = htons((u_short)configuration->GetClusterPort());

            auto client = new ClusterServerClient(addr_in);

            // register epoll
            epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.ptr = client;
            if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, client->GetSocket(), &ev) != 0)
            {
                logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_ctl add failed with code: {}", errno));
            }

            m_clients.insert(std::make_pair(addr, client));
            logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
                "[OnJoinResponse] Added {}:{} to m_clients",
                AddrToStr(addr_in.sin_addr).c_str(),
                ntohs(addr_in.sin_port)
            ));

            client->SendHandshakeRequest();
        }

        packet->Release();
    }

    void ClusterServer::OnHandshakeRequest(ClusterPacket* packet)
    {
        const auto logger = Globals::Get<ILogger>();
        
        logger->Log(LogType::Debug, "ClusterServer", "OnHandshakeRequest...");

        ClusterServerClient* client;

        auto it = m_clients.find(packet->From.sin_addr.s_addr);
        if (it == m_clients.end())
        {
            client = new ClusterServerClient(packet->From);
            
            // register epoll
            epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.ptr = client;
            if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, client->GetSocket(), &ev) != 0)
            {
                logger->Log(LogType::Error, "ClusterServer", nl::String::Format("epoll_ctl add failed with code: {}", errno));
            }

            m_clients.insert(std::make_pair(packet->From.sin_addr.s_addr, client));
            logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
                "[OnHandshakeRequest] Added {}:{} to m_clients",
                AddrToStr(packet->From.sin_addr).c_str(),
                ntohs(packet->From.sin_port)
            ));
        }
        else
        {
            client = it->second;
            client->SetAddress(packet->From); // update address
        }

        packet->Release();

        logger->Log(LogType::Debug, "ClusterServer", "Sent handshake response");
        client->SendHandshakeResponse();
    }
}
