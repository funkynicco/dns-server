using System.Net;
using System.Security.Claims;
using DnsServer.Serializer;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Cors.Infrastructure;

namespace DnsServer;

public static class Extensions
{
        /// <summary>
    /// Adds a logger provider that is being constructed on first use using dependency tree.
    /// </summary>
    public static ILoggingBuilder AddProvider<T>(this ILoggingBuilder builder) where T : class, ILoggerProvider
    {
        builder.Services.AddSingleton<ILoggerProvider, T>();
        return builder;
    }
    
    public static CorsPolicyBuilder WithOriginsFromOptions(this CorsPolicyBuilder policy, IEnumerable<string> origins)
    {
        var to_add = new List<string>();
        foreach (var origin in origins)
        {
            if (origin == "*")
            {
                // we found a * in the origins ; allow any origin
                policy.AllowAnyOrigin();
                return policy;
            }
            
            to_add.Add(origin);
        }

        if (to_add.Count != 0)
        {
            // allow the list of origins
            policy.WithOrigins(to_add.ToArray());
        }
        
        return policy;
    }
    
    public static async Task<bool> IsInPolicy(this ClaimsPrincipal principal, IAuthorizationService authService, string policy)
    {
        var result = await authService.AuthorizeAsync(principal, policy);
        return result.Succeeded;
    }
    
    /// <summary>
    /// Maps a nullable object to a new value. If the value is null, the convert function is not called and default(TType) is returned.
    /// </summary>
    public static TResult? Map<TResult, TType>(this TType? value, Func<TType, TResult> convert)
    {
        if (value is null)
        {
            return default;
        }

        return convert(value);
    }
    
    /// <summary>
    /// Iterates through each item in the <see cref="IEnumerable{T}"/> and executes a user provided action.
    /// </summary>
    public static void ForEach<T>(this IEnumerable<T> enumerable, Action<T> action)
    {
        foreach (var enumerator in enumerable)
        {
            action(enumerator);
        }
    }

    /// <summary>
    /// Iterates through each item in the <see cref="IEnumerable{T}"/> and executes a user provided action.
    /// </summary>
    public static void ForEach<T>(this IEnumerable<T> enumerable, Action<T, int> action)
    {
        var i = 0;
        foreach (var enumerator in enumerable)
        {
            action(enumerator, i++);
        }
    }

    public static Task<T?> ReadFromJsonAsync<T>(this HttpContent content, IJsonSerializer jsonSerializer, CancellationToken cancellationToken = default)
    {
        var options = jsonSerializer.GetOptions();
        return content.ReadFromJsonAsync<T>(options, cancellationToken);
    }

    public static DateTimeOffset StripTime(this DateTimeOffset dto)
    {
        return new DateTimeOffset(dto.Year, dto.Month, dto.Day, 0, 0, 0, dto.Offset);
    }

    public static DateTimeOffset GetNextDay(this DateTimeOffset dto)
    {
        return dto
            .AddDays(1)
            .StripTime();
    }

    public static string Take(this string str, int maxLength)
    {
        maxLength = Math.Min(str.Length, maxLength);
        return str.Substring(0, maxLength);
    }
}