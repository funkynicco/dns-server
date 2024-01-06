using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using DnsServer.Options;
using DnsServer.Utilities;

namespace DnsServer.Hosting.Protocol;

public interface IResolverResponseListener
{
    void OnResolverResponse(
        EndPoint client,
        int transaction_id,
        byte[] data,
        int offset,
        int length,
        TimeSpan elapsed);
}

public class ResolverProxy : IDisposable
{
    class Request : IDisposable
    {
        public long Id { get; set; }
        
        public required EndPoint Client { get; set; }
        
        public int TransactionId { get; set; }
        
        public required Socket Socket { get; set; }

        public Stopwatch RequestTimer { get; } = Stopwatch.StartNew();

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
    private readonly Dictionary<long, RefCounted<Request>> _requests = new();
    private readonly ReaderWriterLockSlim _requestsLock = new(LockRecursionPolicy.NoRecursion);

    private long _ioReceive;
    private long _ioSend;
    private long _nextRequestId;

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
        while (_ioReceive != 0 ||
               _ioSend != 0)
        {
            Thread.Sleep(50);
        }
    }

    public void BeginResolve(
        EndPoint client,
        int transaction_id,
        byte[] data,
        int offset,
        int length)
    {
        var request_id = Interlocked.Increment(ref _nextRequestId);
        
        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        var request = new Request
        {
            Id = request_id,
            Client = client,
            TransactionId = transaction_id,
            Socket = socket
        };
        
        _logger.LogDebug("Begin resolve {TransactionId} from {ClientEndPoint}", transaction_id, client.ToString());

        using var request_ref = RefCounted<Request>.Create(request);

        _requestsLock.EnterWriteLock();
        try
        {
            _requests.Add(
                request.Id,
                request_ref.Copy()
            );
        }
        finally
        {
            _requestsLock.ExitWriteLock();
        }

        // send
        var args = _socketAsyncEventArgsPool.Acquire();
        args.UserToken = request_ref.Copy();
        args.RemoteEndPoint = _proxyEndPoint;
        Buffer.BlockCopy(data, offset, args.Buffer!, 0, length);
        args.UpdateBufferLength(length);
        args.SetCompleted(IO_Sent);
        Interlocked.Increment(ref _ioSend);
        if (!request_ref.Value.Socket.SendToAsync(args))
        {
            IO_Sent(args);
        }
        
        // receive
        args = _socketAsyncEventArgsPool.Acquire();
        args.UserToken = request_ref.Copy();
        args.RemoteEndPoint = _proxyEndPoint;
        args.SetCompleted(IO_Received);
        Interlocked.Increment(ref _ioReceive);
        if (!request_ref.Value.Socket.ReceiveFromAsync(args))
        {
            IO_Received(args);
        }
    }

    private void DataReceived(
        RefCounted<Request> request_ref,
        EndPoint endPoint,
        byte[] data,
        int offset,
        int length)
    {
        var request = request_ref.Value;
        request.RequestTimer.Stop();
        var elapsed = request.RequestTimer.Elapsed;
        
        _listener.OnResolverResponse(
            request.Client,
            request.TransactionId,
            data,
            offset,
            length,
            elapsed
        );
    }

    private void RemoveRequest(long id)
    {
        _requestsLock.EnterWriteLock();
        try
        {
            if (_requests.TryGetValue(id, out var request_ref))
            {
                _requests.Remove(id);
                request_ref.Dispose();
            }
        }
        finally
        {
            _requestsLock.ExitWriteLock();
        }
    }
    
    private void IO_Received(ExtendedSocketAsyncEventArgs args)
    {
        var request_ref = (RefCounted<Request>)args.UserToken!;
        
        _logger.LogDebug(
            "IO_Received {Bytes} from {RemoteEndPoint} (CID: {ClientId} TXID: {TransactionId})",
            args.BytesTransferred,
            args.RemoteEndPoint!.ToString(),
            request_ref.Value.Id,
            request_ref.Value.TransactionId
        );
        
        if (args.SocketError == SocketError.OperationAborted)
        {
            request_ref.Dispose();
            RemoveRequest(request_ref.Value.Id);
            _socketAsyncEventArgsPool.Release(args);
            Interlocked.Decrement(ref _ioReceive);
            return;
        }

        if (args.SocketError == SocketError.Success)
        {
            DataReceived(
                request_ref,
                args.RemoteEndPoint!,
                args.Buffer!,
                args.Offset,
                args.BytesTransferred
            );
        }

        request_ref.Dispose();
        RemoveRequest(request_ref.Value.Id);
        _socketAsyncEventArgsPool.Release(args);
        Interlocked.Decrement(ref _ioReceive);
    }

    private void IO_Sent(ExtendedSocketAsyncEventArgs args)
    {
        var request_ref = (RefCounted<Request>)args.UserToken!;
        
        _logger.LogDebug(
            "IO_Sent {Bytes} to {RemoteEndPoint} (CID: {ClientId} TXID: {TransactionId})",
            args.BytesTransferred,
            args.RemoteEndPoint!.ToString(),
            request_ref.Value.Id,
            request_ref.Value.TransactionId
        );
        
        if (args.SocketError == SocketError.OperationAborted)
        {
            request_ref.Dispose();
            _socketAsyncEventArgsPool.Release(args);
            Interlocked.Decrement(ref _ioSend);
            return;
        }

        request_ref.Dispose();
        _socketAsyncEventArgsPool.Release(args);
        Interlocked.Decrement(ref _ioSend);
    }
}