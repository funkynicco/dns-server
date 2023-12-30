namespace DnsServer;

public static class ApiResults
{
    public static IResult Ok()
    {
        return Results.Ok(new
        {
            result = "ok"
        });
    }

    public static IResult Ok<T>(T value)
    {
        return Results.Ok(new
        {
            result = "ok",
            data = value
        });
    }

    public static IResult Fail(string message)
    {
        return Results.Ok(new
        {
            result = "fail",
            message
        });
    }

    public static IResult ValidationProblem(
        IDictionary<string, string[]> errors,
        string? detail = null,
        string? instance = null,
        int? statusCode = null,
        string? title = null,
        string? type = null,
        IDictionary<string, object?>? extensions = null)
    {
        return Results.ValidationProblem(
            errors,
            detail,
            instance,
            statusCode,
            title,
            type,
            extensions
        );
    }
}