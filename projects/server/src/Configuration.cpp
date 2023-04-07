#include "StdAfx.h"
#include "Configuration.h"

#include <NativeLib/String.h>

Configuration::Configuration()
{
    m_bind_ip = GetEnvOrDefault("BIND_IP", "0.0.0.0");
    m_bind_port = atoi(GetEnvOrDefault("BIND_PORT", "53").c_str());
    m_cluster_key = GetEnvOrDefault("CLUSTER_KEY");
    m_cluster_ip = GetEnvOrDefault("CLUSTER_IP");
    m_cluster_port = atoi(GetEnvOrDefault("CLUSTER_PORT", "6852").c_str());
    m_cluster_subnet = GetEnvOrDefault("CLUSTER_SUBNET", "255.255.255.0");
    m_cluster_broadcast = GetEnvOrDefault("CLUSTER_BROADCAST");
    m_cluster_join_timeout = atoi(GetEnvOrDefault("CLUSTER_JOIN_TIMEOUT", "10").c_str());
}

std::string Configuration::GetEnvOrDefault(const char* name, const char* _default)
{
    const char* value = getenv(name);
    if (!value)
    {
        return _default;
    }

    return value;
}
