using System.Net;
using System.Net.Sockets;
using DnsServer.Options;
using DnsServer.Utilities;

namespace DnsServer.Hosting.Protocol;

public interface IResolverResponseListener
{
    void OnResolverResponse(
        IPEndPoint client,
        int transaction_id,
        byte[] data,
        int offset,
        int length);
}

public class ResolverProxy : IDisposable
{
    class Request : IDisposable
    {
        public required EndPoint Client { get; set; }
        
        public int TransactionId { get; set; }
        
        public required Socket Socket { get; set; }

        public void Dispose()
        {
            Socket.Dispose();
        }
    }
    
    private readonly ILogger _logger;
    private readonly ISocketAsyncEventArgsPool _socketAsyncEventArgsPool;
    private readonly ApiSettings _settings;
    private readonly IResolverResponseListener _listener;
    private readonly EndPoint _proxyEndPoint;

    private long _ioReceive;
    private long _ioSend;

    public ResolverProxy(
        ILogger<ResolverProxy> logger,
        ISocketAsyncEventArgsPool socketAsyncEventArgsPool,
        ApiSettings settings,
        IResolverResponseListener listener)
    {
        _logger = logger;
        _socketAsyncEventArgsPool = socketAsyncEventArgsPool;
        _settings = settings;
        _listener = listener;

        _proxyEndPoint = new IPEndPoint(
            IPAddress.Parse(_settings.DnsServer.Proxy.Address),
            _settings.DnsServer.Proxy.Port
        );
    }
    
    public void Dispose()
    {
    }

    public void BeginResolve(
        EndPoint client,
        int transaction_id,
        byte[] data,
        int offset,
        int length)
    {
        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        var request = new Request
        {
            Client = client,
            TransactionId = transaction_id,
            Socket = socket
        };
        
        var args = _socketAsyncEventArgsPool.Acquire();
        args.UserToken = request;
        args.RemoteEndPoint = _proxyEndPoint;

        //_listener.OnResolverResponse();
    }

    private void DataReceived(EndPoint endPoint, byte[] data, int offset, int length)
    {
    }
    
    private void IO_Received(ExtendedSocketAsyncEventArgs args)
    {
        var request = (Request)args.UserToken!;
        
        if (args.SocketError == SocketError.OperationAborted)
        {
            request.Dispose();
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

        request.Dispose();
        _socketAsyncEventArgsPool.Release(args);
        Interlocked.Decrement(ref _ioReceive);
    }

    private void IO_Sent(ExtendedSocketAsyncEventArgs args)
    {
        var request = (Request)args.UserToken!;
        
        if (args.SocketError == SocketError.OperationAborted)
        {
            request.Dispose();
            _socketAsyncEventArgsPool.Release(args);
            Interlocked.Decrement(ref _ioSend);
            return;
        }

        request.Dispose();
        _socketAsyncEventArgsPool.Release(args);
        Interlocked.Decrement(ref _ioSend);
    }
}