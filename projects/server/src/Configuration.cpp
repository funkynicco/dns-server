#include "StdAfx.h"
#include "Configuration.h"

#include <NativeLib/Parsing/Scanner.h>
#include <NativeLib/File.h>

Configuration::Configuration()
{
    // defaults
    m_bind_ip = "0.0.0.0";
    m_bind_port = 53;
    m_cluster_port = 6852;
    m_cluster_subnet = "255.255.255.0";
    m_cluster_join_timeout = 10;

    // parse configuration file first - if found
    ParseConfigFile("config.cfg");

    // override with environment variables
    ParseEnvironmentVariables();
}

static bool TryCheckAndFetchToken(
    nl::parsing::Scanner& scanner,
    nl::parsing::Token& token,
    const std::string_view name,
    std::string* output)
{
    if (token != name)
    {
        return false;
    }

    token = scanner.Next(); // =
    if (token != "=")
    {
        return false;
    }

    // get the token value
    token = scanner.Next();
    if (!token.IsString())
    {
        return false;
    }

    *output = token.view();
    return true;
}

static bool TryCheckAndFetchToken(
    nl::parsing::Scanner& scanner,
    nl::parsing::Token& token,
    const std::string_view name,
    int* output)
{
    std::string value;
    if (!TryCheckAndFetchToken(scanner, token, name, &value))
    {
        return false;
    }

    *output = atoi(value.c_str());
    return true;
}

void Configuration::ParseConfigFile(const std::string_view filename)
{
    if (!nl::file::Exists(filename))
    {
        return;
    }

    try
    {
        auto scanner = nl::parsing::Scanner::FromFile(filename);
        scanner.EnableExtension(nl::parsing::Extension::HashTagComment);
        
        auto token = scanner.Next();
        while (token)
        {
            TryCheckAndFetchToken(scanner, token, "dns_bind_ip", &m_bind_ip);
            TryCheckAndFetchToken(scanner, token, "dns_bind_port", &m_bind_port);
            TryCheckAndFetchToken(scanner, token, "cluster_key", &m_cluster_key);
            TryCheckAndFetchToken(scanner, token, "cluster_ip", &m_cluster_ip);
            TryCheckAndFetchToken(scanner, token, "cluster_port", &m_cluster_port);
            TryCheckAndFetchToken(scanner, token, "cluster_subnet", &m_cluster_subnet);
            TryCheckAndFetchToken(scanner, token, "cluster_broadcast", &m_cluster_broadcast);
            TryCheckAndFetchToken(scanner, token, "cluster_join_timeout", &m_cluster_join_timeout);
            token = scanner.Next();
        }
    }
    catch (const std::exception&)
    {
        // ignore exceptions as we're only overriding the default values anyway
    }
}

void Configuration::ParseEnvironmentVariables()
{
    TryGetEnvironmentVariable("BIND_IP", &m_bind_ip);
    TryGetEnvironmentVariable("BIND_PORT", &m_bind_port);
    TryGetEnvironmentVariable("CLUSTER_KEY", &m_cluster_key);
    TryGetEnvironmentVariable("CLUSTER_IP", &m_cluster_ip);
    TryGetEnvironmentVariable("CLUSTER_PORT", &m_cluster_port);
    TryGetEnvironmentVariable("CLUSTER_SUBNET", &m_cluster_subnet);
    TryGetEnvironmentVariable("CLUSTER_BROADCAST", &m_cluster_broadcast);
    TryGetEnvironmentVariable("CLUSTER_JOIN_TIMEOUT", &m_cluster_join_timeout);
}

bool Configuration::TryGetEnvironmentVariable(const char* name, int* output)
{
    const auto val = getenv(name);
    if (!val)
    {
        return false;
    }

    *output = atoi(val);
    return true;
}

bool Configuration::TryGetEnvironmentVariable(const char* name, std::string* output)
{
    const auto val = getenv(name);
    if (!val)
    {
        return false;
    }

    *output = val;
    return true;
}
