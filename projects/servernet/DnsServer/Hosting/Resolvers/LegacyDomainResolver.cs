using System.Collections;
using System.Text;
using System.Text.RegularExpressions;
using DnsServer.Utilities;

namespace DnsServer.Hosting.Resolvers;

public class LegacyDomainResolver : BaseDomainResolver
{
    class LegacySettings
    {
        public bool Enabled { get; set; }
        
        public required IEnumerable<string> Blacklists { get; set; }
        
        public required IEnumerable<string> Domains { get; set; }
    }

    struct DomainInformation
    {
        public string Domain { get; init; }
        
        public string Address { get; init; }
        
        public int TimeToLive { get; init; }
    }

    private static readonly Regex _domainLineRegex = new(@"^\d+,(.*?),(.*?),(\d+)$");

    private readonly ILogger _logger;
    private readonly LegacySettings _settings;

    private readonly Dictionary<string, DomainInformation> _domains = new();
    private readonly List<DomainInformation> _wildcardDomains = new();
    private readonly HashSet<string> _blocked = new();
    private readonly List<string> _wildcardBlocked = new();

    private readonly bool _logDebug;
    private readonly bool _logTrace;

    public LegacyDomainResolver(
        ILogger<LegacyDomainResolver> logger,
        IBufferPool bufferPool,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool,
        IConfiguration configuration) : base(bufferPool, socketAsyncEventArgsPool)
    {
        _logger = logger;
        _settings = configuration
            .GetSection("ApiSettings:DnsServer:Resolvers:Legacy")
            .Get<LegacySettings>()!;

        _logDebug = logger.IsEnabled(LogLevel.Debug);
        _logTrace = logger.IsEnabled(LogLevel.Trace);

        if (_settings.Enabled)
        {
            LoadDatabase();
        }
    }

    private void LoadDatabase()
    {
        foreach (var filename in _settings.Domains)
        {
            if (!File.Exists(filename))
            {
                _logger.LogCritical("Database file not found: {Filename}", filename);
                continue;
            }
            
            LoadDatabase(filename);
        }
        
        foreach (var filename in _settings.Blacklists)
        {
            if (!File.Exists(filename))
            {
                _logger.LogCritical("Blacklist file not found: {Filename}", filename);
                continue;
            }
            
            LoadBlacklist(filename);
        }
    }

    private void LoadDatabase(string filename)
    {
        _domains.Clear();
        
        var sb = new StringBuilder(128);
        var text = File.ReadAllText(filename);
        for (var i = 0; i <= text.Length; i++)
        {
            if (i == text.Length ||
                text[i] == '\n')
            {
                if (sb.Length != 0)
                {
                    var line = sb.ToString();
                    var match = _domainLineRegex.Match(line);
                    if (match.Success)
                    {
                        var dns = match.Groups[1].Value.ToLowerInvariant();
                        var ip = match.Groups[2].Value;
                        var ttl = int.Parse(match.Groups[3].Value);
                        AddDomain(dns, ip, ttl);
                    }
                }

                if (i == text.Length)
                {
                    break;
                }

                sb.Clear();
                continue;
            }
            
            if (text[i] == '\r')
            {
                continue;
            }

            sb.Append(text[i]);
        }
    }

    private void LoadBlacklist(string filename)
    {
        _blocked.Clear();

        var sb = new StringBuilder(128);
        var text = File.ReadAllText(filename);
        for (var i = 0; i <= text.Length; i++)
        {
            if (i == text.Length ||
                text[i] == '\n')
            {
                if (sb.Length != 0)
                {
                    AddBlockedDomain(sb.ToString().ToLowerInvariant());
                }

                if (i == text.Length)
                {
                    break;
                }

                sb.Clear();
                continue;
            }
            
            if (text[i] == '\r')
            {
                continue;
            }

            sb.Append(text[i]);
        }
    }

    private void AddBlockedDomain(string domain)
    {
        // check if wildcard
        if (domain.Contains('*'))
        {
            _wildcardBlocked.Add(domain);
        }
        else
        {
            _blocked.Add(domain);
        }
    }

    private void AddDomain(string domain, string ip, int ttl)
    {
        if (domain.Contains('*'))
        {
            _wildcardDomains.Add(new DomainInformation
            {
                Domain = domain,
                Address = ip,
                TimeToLive = ttl
            });
        }
        else
        {
            _domains.Add(domain, new DomainInformation
            {
                Domain = domain,
                Address = ip,
                TimeToLive = ttl
            });
        }
    }

    protected override ResolveResult OnResolve(ref ResolveInformation resolveInformation)
    {
        if (!_settings.Enabled)
        {
            return ResolveResult.Bypass;
        }
        
        var dns = resolveInformation.Domain.ToLowerInvariant();

        // check blocking
        if (_blocked.Contains(dns))
        {
            if (_logTrace)
            {
                _logger.LogTrace("Blocked domain request: {DomainName}", dns);
            }
            
            return ResolveResult.Reject;
        }
        
        // check wildcard blocking
        foreach (var wildcard in _wildcardBlocked)
        {
            if (dns.CompareWildcard(wildcard))
            {
                if (_logTrace)
                {
                    _logger.LogTrace("Blocked domain request: {DomainName} (using wildcard: {WildcardDomain})", dns, wildcard);
                }
                
                return ResolveResult.Reject;
            }
        }
        
        // check domains hashset
        if (_domains.TryGetValue(dns, out var domain_information))
        {
            if (_logTrace)
            {
                _logger.LogTrace(
                    "Resolved domain request: {DomainName} ==> {IPAddress} (TTL: {TimeToLive})",
                    dns,
                    domain_information.Address,
                    domain_information.TimeToLive
                );
            }

            Resolve(ref resolveInformation, domain_information.Address, domain_information.TimeToLive);
            return ResolveResult.Handled;
        }
        
        // check domains wildcard
        foreach (var wildcard in _wildcardDomains)
        {
            if (dns.CompareWildcard(wildcard.Domain))
            {
                if (_logTrace)
                {
                    _logger.LogTrace(
                        "Resolved domain request: {DomainName} ==> {IPAddress} (TTL: {TimeToLive}, Wildcard: {WildcardDomain})",
                        dns,
                        wildcard.Address,
                        wildcard.TimeToLive,
                        wildcard.Domain
                    );
                }
                
                Resolve(ref resolveInformation, wildcard.Address, wildcard.TimeToLive);
                return ResolveResult.Handled;
            }
        }
        
        return ResolveResult.Bypass;
    }
}