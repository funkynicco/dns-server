using DnsServer.Endpoints;
using DnsServer.Hosting;

namespace DnsServer;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddBackgroundService<T>(this IServiceCollection services) where T : IBackgroundService
        => services.AddHostedService<BackgroundServiceManager<T>>();

    public static void AddEndpoints<TImplementation>(this IServiceCollection services) where TImplementation : class, IEndpointsRegisterer
        => services.AddTransient<IEndpointsRegisterer, TImplementation>();
}