namespace DnsServer.Hosting;

public interface IBackgroundService
{
    TimeSpan PollTime { get; }
    
    Task StartupAsync();

    Task ShutdownAsync();

    Task TickAsync(CancellationToken cancellationToken);
}

public class BackgroundServiceManager<T> : BackgroundService where T : IBackgroundService
{
    private readonly IServiceScope _serviceScope;
    
    public BackgroundServiceManager(IServiceProvider services)
    {
        _serviceScope = services.CreateScope();
    }

    public override void Dispose()
    {
        _serviceScope.Dispose();
        base.Dispose();
    }

    protected override async Task ExecuteAsync(CancellationToken cancellationToken)
    {
        var logger = _serviceScope.ServiceProvider.GetRequiredService<ILogger<BackgroundServiceManager<T>>>();
        var service = _serviceScope.ServiceProvider.GetRequiredService<T>();
        var hostApplicationLifetime = _serviceScope.ServiceProvider.GetRequiredService<IHostApplicationLifetime>();

        var ready = false;

        // register an event handler for when application is ready
        hostApplicationLifetime.ApplicationStarted.Register(() => ready = true);

        // wait until kestrel fully started
        var timeout = DateTime.UtcNow.AddSeconds(10);
        while (!ready)
        {
            await Task.Delay(250, cancellationToken);
            if (DateTime.UtcNow >= timeout)
            {
                logger.LogError("Timed out waiting for application ready state!");
                throw new ApplicationException("Timeout waiting for application ready state!");
            }
        }
        
        logger.LogDebug("Starting background service {BackgroundServiceClassName}", typeof(T).Name);

        await RunServiceAsync(cancellationToken, service);
    }

    private async Task RunServiceAsync(CancellationToken cancellationToken, T service)
    {
        await service.StartupAsync();

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                await service.TickAsync(cancellationToken);
                await Task.Delay(service.PollTime, cancellationToken);
            }
        }
        finally
        {
            await service.ShutdownAsync();
        }
    }
}