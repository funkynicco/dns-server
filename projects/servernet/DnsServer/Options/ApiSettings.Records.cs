using DnsServer.Middleware;

namespace DnsServer.Options;

public partial class ApiSettings
{
    public class SetHostMiddlewareOptions : ISetHostMiddlewareSettings
    {
        public required string Hostname { get; set; }

        public bool IsReverseProxyHttps { get; set; }
    }

    public class DnsServerOptions
    {
        public class ProxyOptions
        {
            public required string Address { get; set; }

            public int Port { get; set; }
        }

        public bool Enabled { get; set; }

        public required string BindAddress { get; set; }

        public int BindPort { get; set; }
        
        public required ProxyOptions Proxy { get; set; }
    }
}