using System.Net;
using DnsServer.Hosting.Protocol;
using DnsServer.Utilities;

namespace DnsServer.Hosting.Resolvers;

public enum QueryType
{
    A = 1
}

public enum QueryClass
{
    In = 1
}

public enum ResolveResult
{
    /// <summary>
    /// The DNS request is rejected by returning 0.0.0.0 as IPv4. 
    /// </summary>
    Reject,

    /// <summary>
    /// The DNS request is ghosted; there is no response to the client. 
    /// </summary>
    Ghost,

    /// <summary>
    /// The DNS request is passed along to the next handler.
    /// </summary>
    Bypass,

    /// <summary>
    /// The DNS request is handled by this resolver.
    /// </summary>
    Handled
}

public abstract class BaseDomainResolver
{
    protected struct ResolveInformation
    {
        public IResolverResponseSender Sender { get; init; }
        
        public DnsHeader Header { get; init; }

        public string Domain { get; init; }

        public QueryType QueryType { get; init; }

        public QueryClass QueryClass { get; init; }

        public EndPoint RemoteEndPoint { get; init; }
        
        public PooledBuffer Buffer { get; init; }

        public byte[] OriginalBuffer { get; init; }

        public int OriginalBufferOffset { get; init; }

        public int OriginalBufferLength { get; init; }
    }

    private readonly IBufferPool _bufferPool;
    private readonly ISocketAsyncEventArgsPool _socketAsyncEventArgsPool;

    protected BaseDomainResolver(
        IBufferPool bufferPool,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool)
    {
        _bufferPool = bufferPool;
        _socketAsyncEventArgsPool = socketAsyncEventArgsPool;
    }

    protected abstract ResolveResult OnResolve(ref ResolveInformation resolveInformation);

    public bool HandleRequest(
        IResolverResponseSender sender,
        DnsHeader header,
        string domain,
        QueryType queryType,
        QueryClass queryClass,
        EndPoint remoteEndPoint,
        byte[] originalBuffer,
        int originalOffset,
        int originalLength)
    {
        var buffer = _bufferPool.Acquire();

        var resolve_information = new ResolveInformation
        {
            Sender = sender,
            Header = header,
            Domain = domain,
            QueryType = queryType,
            QueryClass = queryClass,
            RemoteEndPoint = remoteEndPoint,
            Buffer = buffer,
            OriginalBuffer = originalBuffer,
            OriginalBufferOffset = originalOffset,
            OriginalBufferLength = originalLength,
        };

        var result = OnResolve(ref resolve_information);
        if (result == ResolveResult.Bypass)
        {
            // pass along to next resolver; or dns server
            _bufferPool.Release(buffer);
            return false;
        }

        switch (result)
        {
            case ResolveResult.Handled:
                // buffer ??
                break;
            case ResolveResult.Ghost:
                // do nothing
                break;
            case ResolveResult.Reject:
                // set buffer to rejection dns
                Resolve(ref resolve_information, 0, 60);
                break;
        }

        _bufferPool.Release(buffer);
        return true;
    }

    protected void Resolve(ref ResolveInformation resolveInformation, string ip, int ttl)
    {
        var bytes = new int[4];
        var j = 0;
        for (var i = 0; i < ip.Length; i++)
        {
            if (ip[i] == '.')
            {
                ++j;
                continue;
            }

            bytes[j] = bytes[j] * 10 + (ip[i] - '0');
        }

        var n = bytes[0] << 24 |
                bytes[1] << 16 |
                bytes[2] << 8 |
                bytes[3];

        Resolve(ref resolveInformation, n, ttl);
    }
    
    protected void Resolve(ref ResolveInformation resolveInformation, int ip, int ttl)
    {
        var args = _socketAsyncEventArgsPool.Acquire();
        var response = args.Buffer!;
        var response_offset = DnsHeader.Size;

        var response_header = resolveInformation.Header;
        response_header.Flags |= 1 << 15; // QR 1 for response
        response_header.NumberOfAnswers = 1;
        response_header.Write(response, 0); // override original buffer

        // copy the current packet question into response
        Buffer.BlockCopy(resolveInformation.OriginalBuffer, DnsHeader.Size, response, response_offset, resolveInformation.OriginalBufferLength - DnsHeader.Size);
        response_offset += resolveInformation.OriginalBufferLength - DnsHeader.Size;

        // add answer here...
        // 4.1.4. of https://www.ietf.org/rfc/rfc1035.txt - offset in this packet with 2 high bits set for the DNS name
        /*
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            | 1  1|                OFFSET                   |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
         */
        var name_offset = 12 | (3 << 14); // offset is 12 from start of packet, then OR the two high bits
        response[response_offset++] = (byte)(name_offset >> 8);
        response[response_offset++] = (byte)(name_offset & 0xff);

        // response type
        response[response_offset++] = 0;
        response[response_offset++] = 1;

        // response class
        response[response_offset++] = 0;
        response[response_offset++] = 1;

        // TTL as int; value in seconds
        response[response_offset++] = (byte)(ttl >> 24);
        response[response_offset++] = (byte)(ttl >> 16);
        response[response_offset++] = (byte)(ttl >> 8);
        response[response_offset++] = (byte)(ttl & 0xff);

        // data length
        response[response_offset++] = 0;
        response[response_offset++] = 4; // ipv4 address

        // ipv4 address
        response[response_offset++] = (byte)((ip >> 24) & 0xff);
        response[response_offset++] = (byte)((ip >> 16) & 0xff);
        response[response_offset++] = (byte)((ip >> 8) & 0xff);
        response[response_offset++] = (byte)(ip & 0xff);

        // send
        args.UpdateBufferLength(response_offset);
        args.RemoteEndPoint = resolveInformation.RemoteEndPoint;
        resolveInformation.Sender.SendResponse(args);
    }
}