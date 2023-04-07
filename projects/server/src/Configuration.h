#pragma once

class Configuration
{
public:
    Configuration();
    
    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) noexcept = delete;
    Configuration& operator =(const Configuration&) = delete;
    Configuration& operator =(Configuration&&) noexcept = delete;

    const std::string& GetBindIP() const { return m_bind_ip; }
    int GetBindPort() const { return m_bind_port; }
    const std::string& GetClusterKey() const { return m_cluster_key; }
    const std::string& GetClusterIP() const { return m_cluster_ip; }
    int GetClusterPort() const { return m_cluster_port; }
    const std::string& GetClusterSubnet() const { return m_cluster_subnet; }
    const std::string& GetClusterBroadcast() const { return m_cluster_broadcast; }
    int GetClusterJoinTimeout() const { return m_cluster_join_timeout; }

    static std::string GetEnvOrDefault(const char* name, const char* _default = "");

private:
    std::string m_bind_ip;
    int m_bind_port;
    std::string m_cluster_key;
    std::string m_cluster_ip;
    int m_cluster_port;
    std::string m_cluster_subnet;
    std::string m_cluster_broadcast;
    int m_cluster_join_timeout;
};