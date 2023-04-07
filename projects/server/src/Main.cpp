#include "StdAfx.h"
#include "Network/WindowsWsaBootstrap.h"
#include "Network/NetUtils.h"

#include "Network/Cluster/ClusterServer.h"
#include "Network/Dns/DnsServer.h"
#include "Network/Web/WebServer.h"

#include "Network/PacketSequence.h"

#include <NativeLib/SystemLayer/SystemLayer.h>

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

void ShowHelp(ILogger* logger);

static bool s_shutdown = false;

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
    sigaction(SIGTERM, &handler, nullptr);
#endif
}

int RunServer(ILogger* logger, bool enable_dns_server, const Configuration& configuration);

int main(int argc, char** argv)
{
    InstallCtrlCHandler();

    nl::systemlayer::SetDefaultSystemLayerFunctions();

    // disable console buffering for docker
    setvbuf(stdout, nullptr, _IONBF, 0);

    //TestSequence();
    //return 0;
    logging::Logger logger;

    Configuration configuration;

    if (argc == 1)
    {
        ShowHelp(&logger);
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-h") == 0)
        {
            ShowHelp(&logger);
        }
        else
        {
            // command
            auto enable_dns_server = false;
            if (strcmp(argv[i], "server") == 0)
            {
                enable_dns_server = true;
            }
            else if (strcmp(argv[i], "backup") == 0)
            {
            }
            else
            {
                logger.Log(LogType::Error, "Main", nl::String::Format("Invalid argument: '%s'", argv[i]));
                return 1;
            }

            return RunServer(&logger, enable_dns_server, configuration);
        }
    }

    return 0;
}

void ShowHelp(ILogger* logger)
{
    logger->Log(LogType::Info, "Help", "Usage: clusterdns [options] <command>");
    logger->Log(LogType::Info, "Help", "Command can be one of:");
    logger->Log(LogType::Info, "Help", "  server    - Runs the DNS server and serves requests");
    logger->Log(LogType::Info, "Help", "  backup    - Runs only the cluster backup service to distribute database");
}

int RunServer(ILogger* logger, bool enable_dns_server, const Configuration& configuration)
{
#ifdef _WIN32
    network::WsaBootstrap wsa;
#endif

    auto cluster_server = network::cluster::ClusterServer(configuration, logger);

    // only allow packets from the cluster ip subnet
    cluster_server
        .GetIPv4Filter()
        .AddRule(configuration.GetClusterIP().c_str(), configuration.GetClusterSubnet().c_str());

    cluster_server.SetMyAddress(network::MakeAddr(configuration.GetClusterIP().c_str()));

    try
    {
        // on linux, broadcast server must listen on 0.0.0.0,
        // so we have to filter the incoming packets by their source IP to match correct network
        cluster_server.Start(network::MakeAddr("0.0.0.0", configuration.GetClusterPort()), true);
    }
    catch (const std::exception& ex)
    {
        logger->Log(LogType::Error, "Server", nl::String::Format("Failed to start cluster server. Message: {}", ex.what()));
        return 1;
    }

    logger->Log(LogType::Info, "Server", nl::String::Format("Started cluster server on 0.0.0.0:{} (broadcast: {})",
        configuration.GetClusterPort(),
        configuration.GetClusterBroadcast().c_str()));

    auto dns_server = network::dns::DnsServer(configuration, logger);
    auto web_server = network::web::WebServer(configuration, logger);
    if (enable_dns_server)
    {
        try
        {
            dns_server.Start(network::MakeAddr(configuration.GetBindIP().c_str(), configuration.GetBindPort()));
        }
        catch (const std::exception& ex)
        {
            logger->Log(LogType::Error, "Server", nl::String::Format("Failed to start dns server. Message: {}", ex.what()));
            return 1;
        }

        logger->Log(LogType::Info, "Server", nl::String::Format("Started DNS server on {}:{}", configuration.GetBindIP().c_str(), configuration.GetBindPort()));

        try
        {
            web_server.Start(network::MakeAddr("0.0.0.0", 8550));
        }
        catch (const std::exception& ex)
        {
            logger->Log(LogType::Error, "Server", nl::String::Format("Failed to start web server. Message: {}", ex.what()));
            return 1;
        }

        logger->Log(LogType::Info, "Server", nl::String::Format("Started Web server on 0.0.0.0:{}", 8550));
    }

    while (!s_shutdown)
    {
        cluster_server.Process();

        if (enable_dns_server)
        {
            dns_server.Process();
            web_server.Process();
        }

        Sleep(100);
    }

    logger->Log(LogType::Info, "Server", "Shutting down ...");
    return 0;
}