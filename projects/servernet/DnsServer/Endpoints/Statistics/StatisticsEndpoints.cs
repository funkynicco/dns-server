namespace DnsServer.Endpoints.Statistics;

public class StatisticsEndpoints : IEndpointsRegisterer
{
    public void RegisterEndpoints(IEndpointRouteBuilder app)
    {
        app.MapGet("/api/statistics", GetStatistics);
    }

    private static IResult GetStatistics()
    {
        return ApiResults.Ok(new
        {
            Hello = "world"
        });
    }
}