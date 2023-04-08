#pragma once

class Configuration
{
public:
    Configuration();
    
    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) noexcept = delete;
    Configuration& operator =(const Configuration&) = delete;
    Configuration& operator =(Configuration&&) noexcept = delete;

    [[nodiscard]] const std::string& GetBindIP() const { return m_bind_ip; }
    [[nodiscard]] int GetBindPort() const { return m_bind_port; }
    [[nodiscard]] const std::string& GetClusterKey() const { return m_cluster_key; }
    [[nodiscard]] const std::string& GetClusterIP() const { return m_cluster_ip; }
    [[nodiscard]] int GetClusterPort() const { return m_cluster_port; }
    [[nodiscard]] const std::string& GetClusterSubnet() const { return m_cluster_subnet; }
    [[nodiscard]] const std::string& GetClusterBroadcast() const { return m_cluster_broadcast; }
    [[nodiscard]] int GetClusterJoinTimeout() const { return m_cluster_join_timeout; }

private:
    void ParseConfigFile(std::string_view filename);
    void ParseEnvironmentVariables();

    static bool TryGetEnvironmentVariable(const char* name, int* output);
    static bool TryGetEnvironmentVariable(const char* name, std::string* output);
    
    std::string m_bind_ip;
    int m_bind_port;
    std::string m_cluster_key;
    std::string m_cluster_ip;
    int m_cluster_port;
    std::string m_cluster_subnet;
    std::string m_cluster_broadcast;
    int m_cluster_join_timeout;
};