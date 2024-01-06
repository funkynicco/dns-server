using System.Net;
using System.Net.Sockets;
using System.Text;
using DnsServer.Hosting.Protocol;
using DnsServer.Hosting.Resolvers;
using DnsServer.Options;
using DnsServer.Utilities;
using Microsoft.Extensions.Options;

namespace DnsServer.Hosting;

public interface IDomainResolverServer : IBackgroundService
{
}

public interface IResolverResponseSender
{
    void SendResponse(ExtendedSocketAsyncEventArgs args);
}

public class DomainResolverServer : IDomainResolverServer, IResolverResponseListener, IResolverResponseSender
{
    private readonly ApiSettings _settings;
    private readonly ILogger _logger;
    private readonly EndPoint _dnsServerEndPoint;
    private readonly ISocketAsyncEventArgsPool _socketAsyncEventArgsPool;
    private readonly ResolverProxy _resolverProxy;
    private readonly IEnumerable<BaseDomainResolver> _resolvers;

    private Socket? _socket;
    private long _ioReceive;
    private long _ioSend;

    private readonly bool _logDebug;
    private readonly bool _logTrace;

    public TimeSpan PollTime => TimeSpan.FromSeconds(1);

    public DomainResolverServer(
        IOptions<ApiSettings> settings,
        ILogger<DomainResolverServer> logger,
        ILogger<ResolverProxy> resolverProxyLogger,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool,
        IEnumerable<BaseDomainResolver> resolvers)
    {
        _settings = settings.Value;
        _logger = logger;
        _socketAsyncEventArgsPool = socketAsyncEventArgsPool;
        _resolvers = resolvers;

        _dnsServerEndPoint = new IPEndPoint(
            IPAddress.Parse(settings.Value.DnsServer.BindAddress),
            settings.Value.DnsServer.BindPort
        );

        _resolverProxy = new(
            resolverProxyLogger,
            socketAsyncEventArgsPool,
            settings.Value,
            this
        );

        _logDebug = _logger.IsEnabled(LogLevel.Debug);
        _logTrace = _logger.IsEnabled(LogLevel.Trace);
    }

    public Task StartupAsync()
    {
        _logger.LogTrace("Domain Resolver startup");

        if (!_settings.DnsServer.Enabled)
        {
            return Task.CompletedTask;
        }

        _socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        _socket.Bind(_dnsServerEndPoint);
        
        var args = _socketAsyncEventArgsPool.Acquire();
        args.RemoteEndPoint = new IPEndPoint(IPAddress.Any, 0);
        args.SetCompleted(IO_Received);
        Interlocked.Increment(ref _ioReceive);
        if (!_socket.ReceiveFromAsync(args))
        {
            IO_Received(args);
        }

        return Task.CompletedTask;
    }

    public Task ShutdownAsync()
    {
        _logger.LogTrace("Domain Resolver shutdown");

        if (_socket is not null)
        {
            _socket.Dispose();
            _socket = null;
        }

        while (_ioReceive != 0 ||
               _ioSend != 0)
        {
            Thread.Sleep(50);
        }
        
        _resolverProxy.Dispose();

        return Task.CompletedTask;
    }

    public Task TickAsync(CancellationToken cancellationToken)
    {
        if (!_settings.DnsServer.Enabled)
        {
            return Task.CompletedTask;
        }

        // ...

        return Task.CompletedTask;
    }

    ////////////////////////////////////////////////////////////////

    private string ReadDomainLabels(byte[] data, ref int i, int end)
    {
        var sb = new StringBuilder(128);

        do
        {
            var len = data[i++];
            if (len == 0)
            {
                break;
            }

            if (i + len > end)
            {
                throw new CorruptedPacketException();
            }

            if (sb.Length != 0)
            {
                sb.Append('.');
            }

            sb.Append(Encoding.ASCII.GetString(data, i, len));
            i += len;
        } while (true);

        return sb.ToString();
    }

    private void DataReceived(EndPoint endPoint, byte[] data, int offset, int length)
    {
        if (length < DnsHeader.Size)
        {
            _logger.LogDebug("Invalid DNS packet size");
            return;
        }

        var header = DnsHeader.Decode(data, offset);
        if (header.NumberOfQuestions == 0)
        {
            _logger.LogDebug("DNS packet contained no questions!");
            return;
        }

        if (header.NumberOfQuestions > 1)
        {
            _logger.LogDebug("DNS packet contained more than one question (unsupported)!");
            return;
        }

        var i = offset + DnsHeader.Size;
        var end = offset + length;
        var domain = ReadDomainLabels(data, ref i, end);
        var query_type = data[i] << 8 | data[i + 1];
        var query_class = data[i + 2] << 8 | data[i + 3];
        
        // query type:
        // 1 - A record
        // 5 - CNAME, typically used in response to point to another alias; the chain of cname-->type A records are typically responded in same
        // 65 - HTTPS; resolve dns to https endpoint
        
        // class:
        // 1 - IN

        if (query_type == 1 && // A record ipv4 and IN class
            query_class == 1 &&
            !domain.EndsWith(".arpa"))
        {
            if (_logDebug)
            {
                _logger.LogDebug(
                    "Resolve ID:{TransactionId} Flags:{Flags} DNS:{DNS} Type:{Type} Class:{Class} -- FROM: {From}",
                    header.TransactionId,
                    header.Flags,
                    domain,
                    query_type.ToString("x4"),
                    query_class.ToString("x4"),
                    endPoint.ToString()
                );
            }
            
            // resolve using resolvers...

            foreach (var resolver in _resolvers)
            {
                if (resolver.HandleRequest(
                        this,
                        header,
                        domain,
                        (QueryType)query_type,
                        (QueryClass)query_class,
                        endPoint,
                        data,
                        offset,
                        length))
                {
                    // short circuit the request; handled...
                    return;
                }
            }
        }

        // type should only be 0x0001 - (A) Record host address and class IN (0x0001)

        // read domain labels until we get 0x00
        // the next bytes are Type and Class

        // Type: 0x000c - PTR
        // Class: IN (0x0001)

        ////////////////////////////////////////////////////////

        _resolverProxy.BeginResolve(
            endPoint,
            header.TransactionId,
            data,
            offset,
            length
        );
    }

    ////////////////////////////////////////////////////////////////

    private void IO_Received(ExtendedSocketAsyncEventArgs args)
    {
        if (args.SocketError == SocketError.OperationAborted)
        {
            _socketAsyncEventArgsPool.Release(args);
            Interlocked.Decrement(ref _ioReceive);
            return;
        }

        if (args.SocketError == SocketError.Success)
        {
            DataReceived(
                args.RemoteEndPoint!,
                args.Buffer!,
                args.Offset,
                args.BytesTransferred
            );
        }

        args.RemoteEndPoint = new IPEndPoint(IPAddress.Any, 0);
        args.ResetBuffer();
        if (!_socket!.ReceiveFromAsync(args))
        {
            IO_Received(args);
        }
    }

    private void IO_Sent(ExtendedSocketAsyncEventArgs args)
    {
        if (args.SocketError == SocketError.OperationAborted)
        {
            _socketAsyncEventArgsPool.Release(args);
            Interlocked.Decrement(ref _ioSend);
            return;
        }

        _socketAsyncEventArgsPool.Release(args);
        Interlocked.Decrement(ref _ioSend);
    }

    public void OnResolverResponse(EndPoint client, int transaction_id, byte[] data, int offset, int length, TimeSpan elapsed)
    {
        // send result back to client
        
        var args = _socketAsyncEventArgsPool.Acquire();
        var buffer = args.Buffer!;

        Buffer.BlockCopy(data, offset, buffer, 0, length);
        args.UpdateBufferLength(length);
        args.RemoteEndPoint = client;
        args.SetCompleted(IO_Sent);
        Interlocked.Increment(ref _ioSend);
        if (!_socket!.SendToAsync(args))
        {
            IO_Sent(args);
        }

        if (_logTrace)
        {
            _logger.LogTrace("TXID: {TransactionId} - Resolved in {Elapsed}", transaction_id, elapsed.ToString());
        }
    }

    public void SendResponse(ExtendedSocketAsyncEventArgs args)
    {
        args.SetCompleted(IO_Sent);
        Interlocked.Increment(ref _ioSend);
        if (!_socket!.SendToAsync(args))
        {
            IO_Sent(args);
        }
    }
}

public class CorruptedPacketException : Exception
{
    public CorruptedPacketException() :
        base("The packet appears corrupted.")
    {
    }
}