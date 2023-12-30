namespace DnsServer.Endpoints;

public interface IEndpointsRegisterer
{
    void RegisterEndpoints(IEndpointRouteBuilder app);
}