namespace DnsServer.Options;

public partial class ApiSettings
{
    public required List<string> CorsAllowOrigins { get; set; }
    
    public required SetHostMiddlewareOptions SetHostMiddleware { get; set; }
    
    public required DnsServerOptions DnsServer { get; set; }
}