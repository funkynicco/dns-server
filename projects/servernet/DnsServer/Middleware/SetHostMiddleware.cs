namespace DnsServer.Middleware;

public interface ISetHostMiddlewareSettings
{
    string Hostname { get; }
    
    bool IsReverseProxyHttps { get; }
}

public class SetHostMiddleware
{
    private readonly RequestDelegate _next;
    private readonly ISetHostMiddlewareSettings _settings;

    public SetHostMiddleware(RequestDelegate next, ISetHostMiddlewareSettings settings)
    {
        _next = next;
        _settings = settings;
    }

    public async Task Invoke(HttpContext context)
    {
        if (_settings.IsReverseProxyHttps)
        {
            context.Request.Scheme = "https";
        }

        if (!string.IsNullOrEmpty(_settings.Hostname))
        {
            context.Request.Host = new HostString(_settings.Hostname);
        }

        await _next(context);
    }
}