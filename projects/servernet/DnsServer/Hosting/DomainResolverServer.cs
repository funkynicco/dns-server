using System.Net;
using System.Net.Sockets;
using System.Text;
using DnsServer.Hosting.Protocol;
using DnsServer.Options;
using DnsServer.Utilities;
using Microsoft.Extensions.Options;

namespace DnsServer.Hosting;

public interface IDomainResolverServer : IBackgroundService
{
}

public class DomainResolverServer : IDomainResolverServer, IResolverResponseListener
{
    enum AsyncType
    {
        Client,
        Proxy
    }
    
    private readonly ApiSettings _settings;
    private readonly ILogger _logger;
    private readonly EndPoint _dnsServerEndPoint;
    private readonly ISocketAsyncEventArgsPool _socketAsyncEventArgsPool;
    private readonly ResolverProxy _resolverProxy;
    
    private Socket? _socket;
    private long _ioReceive;
    private long _ioSend;

    public TimeSpan PollTime => TimeSpan.FromSeconds(1);

    public DomainResolverServer(
        IOptions<ApiSettings> settings,
        ILogger<DomainResolverServer> logger,
        ILogger<ResolverProxy> resolverProxyLogger,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool)
    {
        _settings = settings.Value;
        _logger = logger;
        _socketAsyncEventArgsPool = socketAsyncEventArgsPool;

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
        args.UserToken = AsyncType.Client;
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

        if (!domain.EndsWith(".arpa"))
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

            // if (query_type == 1 && // A record ipv4 and IN class
            //     query_class == 1)
            {
                var args = _socketAsyncEventArgsPool.Acquire();
                var response = args.Buffer!;
                var response_offset = DnsHeader.Size;

                var response_header = header;
                response_header.Flags = 1 << 15; // QR 1 for response
                response_header.NumberOfAnswers = 1;
                response_header.Write(response, 0);

                // copy the current packet question into response
                Buffer.BlockCopy(data, DnsHeader.Size, response, response_offset, length - DnsHeader.Size);
                response_offset += length - DnsHeader.Size;

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
                var ttl = 300; // 5min
                response[response_offset++] = (byte)(ttl >> 24);
                response[response_offset++] = (byte)(ttl >> 16);
                response[response_offset++] = (byte)(ttl >> 8);
                response[response_offset++] = (byte)(ttl & 0xff);

                // data length
                response[response_offset++] = 0;
                response[response_offset++] = 4; // ipv4 address

                // ipv4 address (10.20.30.40)
                response[response_offset++] = 10;
                response[response_offset++] = 20;
                response[response_offset++] = 30;
                response[response_offset++] = 40;
                
                // send
                args.UpdateBufferLength(response_offset);
                args.RemoteEndPoint = endPoint;
                args.SetCompleted(IO_Sent);
                Interlocked.Increment(ref _ioSend);
                if (!_socket!.SendToAsync(args))
                {
                    IO_Sent(args);
                }

                return;
            }
        }

        // type should only be 0x0001 - (A) Record host address and class IN (0x0001)

        // read domain labels until we get 0x00
        // the next bytes are Type and Class

        // Type: 0x000c - PTR
        // Class: IN (0x0001)

        ////////////////////////////////////////////////////////

        // respond with echoing

        // var new_data = new byte[65535];
        // Buffer.BlockCopy(data, offset, new_data, 0, length);
        //
        // var args = new SocketAsyncEventArgs();
        // args.SetBuffer(new_data, 0, length);
        // args.RemoteEndPoint = endPoint;
        // args.Completed += IO_Sent;
        // Interlocked.Increment(ref _ioSend);
        // if (!_socket!.SendToAsync(args))
        // {
        //     IO_Sent(this, args);
        // }

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

    public void OnResolverResponse(IPEndPoint client, int transaction_id, byte[] data, int offset, int length)
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
    }
}

public class CorruptedPacketException : Exception
{
    public CorruptedPacketException() :
        base("The packet appears corrupted.")
    {
    }
}