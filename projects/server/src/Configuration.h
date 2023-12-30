#pragma once

#include "StdAfx.h"

struct IConfiguration
{
    IConfiguration() = default;
    virtual ~IConfiguration() = default;

    DEFINE_COPY_MOVE_DELETE(IConfiguration);
    
    [[nodiscard]] virtual const std::string& GetBindIP() const = 0;
    [[nodiscard]] virtual int GetBindPort() const = 0;
    [[nodiscard]] virtual const std::string& GetClusterKey() const = 0;
    [[nodiscard]] virtual const std::string& GetClusterIP() const = 0;
    [[nodiscard]] virtual int GetClusterPort() const = 0;
    [[nodiscard]] virtual const std::string& GetClusterSubnet() const = 0;
    [[nodiscard]] virtual const std::string& GetClusterBroadcast() const = 0;
    [[nodiscard]] virtual int GetClusterJoinTimeout() const = 0;
    [[nodiscard]] virtual const std::string& GetWebBindIP() const = 0;
    [[nodiscard]] virtual int GetWebBindPort() const = 0;
};

class Configuration final : public IConfiguration
{
public:
    Configuration();
    ~Configuration() override = default;

    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) noexcept = delete;
    Configuration& operator =(const Configuration&) = delete;
    Configuration& operator =(Configuration&&) noexcept = delete;

    [[nodiscard]] const std::string& GetBindIP() const override { return m_bind_ip; }
    [[nodiscard]] int GetBindPort() const override { return m_bind_port; }
    [[nodiscard]] const std::string& GetClusterKey() const override { return m_cluster_key; }
    [[nodiscard]] const std::string& GetClusterIP() const override { return m_cluster_ip; }
    [[nodiscard]] int GetClusterPort() const override { return m_cluster_port; }
    [[nodiscard]] const std::string& GetClusterSubnet() const override { return m_cluster_subnet; }
    [[nodiscard]] const std::string& GetClusterBroadcast() const override { return m_cluster_broadcast; }
    [[nodiscard]] int GetClusterJoinTimeout() const override { return m_cluster_join_timeout; }
    [[nodiscard]] const std::string& GetWebBindIP() const override { return m_web_bind_ip; }
    [[nodiscard]] int GetWebBindPort() const override { return m_web_bind_port; }

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
    std::string m_web_bind_ip;
    int m_web_bind_port;
};