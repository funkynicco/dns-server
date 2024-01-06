using System.Text.Json;
using DnsServer;
using DnsServer.Endpoints;
using DnsServer.Endpoints.Statistics;
using DnsServer.Hosting;
using DnsServer.Hosting.Resolvers;
using DnsServer.Middleware;
using DnsServer.Options;
using DnsServer.Serializer;
using DnsServer.Utilities;
using Microsoft.AspNetCore.Http.Json;

var builder = WebApplication.CreateBuilder(args);
//builder.Configuration.AddYamlFile("appsettings.test.yaml", optional: false);
if (builder.Environment.IsDevelopment())
{
    builder.Configuration.AddJsonFile("appsettings.local.json", optional: true, reloadOnChange: true);
}

builder.Configuration.AddEnvironmentVariables();

var api_settings = builder.Configuration.GetSection(nameof(ApiSettings)).Get<ApiSettings>()!;

builder.Services.Configure<JsonOptions>(options =>
{
    options.SerializerOptions.PropertyNamingPolicy = new JsonSnakeCaseNamingPolicy();
    if (builder.Environment.IsDevelopment())
    {
        options.SerializerOptions.WriteIndented = true;
    }
});

builder.Services.AddSingleton<IJsonSerializer>(new JsonSnakeCaseSerializer(new JsonSerializerOptions
{
    PropertyNamingPolicy = new JsonSnakeCaseNamingPolicy()
}));

// map valplattform appsettings to IOptions<ValplattformOptions>
builder.Services.Configure<ApiSettings>(builder.Configuration.GetSection(nameof(ApiSettings)));

// general services
builder.Services.AddHttpContextAccessor();

// custom services
builder.Services.AddSingleton<ISocketAsyncEventArgsPool, SocketAsyncEventArgsPool>();
builder.Services.AddSingleton<IDomainResolverServer, DomainResolverServer>();
builder.Services.AddSingleton<IBufferPool>(_ => new BufferPool(8, 8192));

// background services
builder.Services.AddBackgroundService<IDomainResolverServer>();

// endpoints
builder.Services.AddEndpoints<StatisticsEndpoints>();

// domain resolvers (the order matters here)
builder.Services.AddDomainResolver<LegacyDomainResolver>();
builder.Services.AddDomainResolver<TestDomainResolver>();

////////////////////////////////////////////////////////////

var app = builder.Build();

app.UseMiddleware<SetHostMiddleware>(api_settings.SetHostMiddleware);
app.UseStaticFiles();
app.UseRouting();
app.UseDefaultFiles();

// register all endpoints we defined with AddEndpoints above
app.Services.GetServices<IEndpointsRegisterer>().ForEach(a => a.RegisterEndpoints(app));
app.UseEndpoints(_ => { }); // microsoft workaround for injecting the Endpoints middleware before SPAMiddleware

// no endpoints handled the request; send request to index.html
app.MapFallbackToFile("index.html");

app.Run();