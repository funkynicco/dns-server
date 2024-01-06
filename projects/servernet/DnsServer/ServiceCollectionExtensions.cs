using DnsServer.Endpoints;
using DnsServer.Hosting;
using DnsServer.Hosting.Resolvers;

namespace DnsServer;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddBackgroundService<T>(this IServiceCollection services) where T : IBackgroundService
        => services.AddHostedService<BackgroundServiceManager<T>>();

    public static IServiceCollection AddEndpoints<TImplementation>(this IServiceCollection services) where TImplementation : class, IEndpointsRegisterer
        => services.AddTransient<IEndpointsRegisterer, TImplementation>();

    public static IServiceCollection AddDomainResolver<T>(this IServiceCollection services) where T : BaseDomainResolver
    {
        services.AddSingleton<BaseDomainResolver, T>();
        return services;
    }
}