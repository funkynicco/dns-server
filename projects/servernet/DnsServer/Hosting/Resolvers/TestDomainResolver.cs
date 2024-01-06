using DnsServer.Utilities;

namespace DnsServer.Hosting.Resolvers;

public class TestDomainResolver : BaseDomainResolver
{
    public TestDomainResolver(
        IBufferPool bufferPool,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool) : base(bufferPool, socketAsyncEventArgsPool)
    {
    }
    
    protected override ResolveResult OnResolve(ref ResolveInformation resolveInformation)
    {
        // if (resolveInformation.Domain == "e.hello.com")
        // {
        //     return ResolveResult.Reject;
        // }
        
        return ResolveResult.Bypass;
    }
}