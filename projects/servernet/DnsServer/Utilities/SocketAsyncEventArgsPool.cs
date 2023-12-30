//#define DISABLE_POOL

using System.Net;
using System.Net.Sockets;
using System.Reflection;

namespace DnsServer.Utilities;

public class ExtendedSocketAsyncEventArgs : SocketAsyncEventArgs
{
    private readonly byte[] _buffer;
    private Action<ExtendedSocketAsyncEventArgs>? _completed;

    public int Id { get; }

    public ExtendedSocketAsyncEventArgs(int id, int bufferSize)
    {
        _buffer = new byte[bufferSize];
        SetBuffer(_buffer, 0, _buffer.Length);
        
        Id = id;
    }

    public void UpdateBufferLength(int length)
    {
        SetBuffer(_buffer, 0, length);
    }

    public void ResetBuffer()
    {
        SetBuffer(_buffer, 0, _buffer.Length);
    }

    public void SetCompleted(Action<ExtendedSocketAsyncEventArgs> callback)
    {
        _completed = callback;
    }

    protected override void OnCompleted(SocketAsyncEventArgs args)
    {
        _completed?.Invoke((ExtendedSocketAsyncEventArgs)args);
    }
}

public interface ISocketAsyncEventArgsPool
{
    ExtendedSocketAsyncEventArgs Acquire();

    void Release(ExtendedSocketAsyncEventArgs args);
}

public class SocketAsyncEventArgsPool : ISocketAsyncEventArgsPool, IDisposable
{
    private readonly ILogger _logger;
    private readonly int _maxPoolSize;
    private readonly int _bufferSize;
    private readonly FieldInfo _socketAddressField;

    private readonly ReaderWriterLockSlim _lock = new(LockRecursionPolicy.NoRecursion);
    private ExtendedSocketAsyncEventArgs[] _pool = new ExtendedSocketAsyncEventArgs[8];
    private int _count;
    private int _allocated;

    private int _nextId;

    public SocketAsyncEventArgsPool(
        ILogger<SocketAsyncEventArgsPool> logger,
        int maxPoolSize = 256,
        int bufferSize = 4096)
    {
        _logger = logger;
        _maxPoolSize = maxPoolSize;
        _bufferSize = bufferSize;
        
        // try to find the _socketAddress field that is internal in the SocketAsyncEventArgs class
        // this value has to be patched to null when a SocketAsyncEventArgs is returned to the pool
        var socketAddressField = typeof(ExtendedSocketAsyncEventArgs).GetField(
            "_socketAddress",
            BindingFlags.NonPublic | BindingFlags.Instance
        );

        _socketAddressField = socketAddressField ?? throw new InvalidProgramException($"The {nameof(ExtendedSocketAsyncEventArgs)} class does not contain _socketAddress field.");
    }

    public void Dispose()
    {
        if (_count < _allocated)
        {
            _logger.LogCritical(
                "Dispose: Only {Count} of {Allocated} pooled objects were returned during disposing",
                _count,
                _allocated
            );
        }

        for (var i = 0; i < _count; i++)
        {
            _pool[i].Dispose();
        }
        
        _lock.Dispose();
    }

    public ExtendedSocketAsyncEventArgs Acquire()
    {
#if DISABLE_POOL
        return new(Interlocked.Increment(ref _nextId), _bufferSize);
#endif

        ExtendedSocketAsyncEventArgs? args = null;

        _lock.EnterWriteLock();
        try
        {
            if (_count > 0)
            {
                args = _pool[--_count];
            }
        }
        finally
        {
            _lock.ExitWriteLock();
        }

        if (args is null)
        {
            Interlocked.Increment(ref _allocated);
            return new(
                Interlocked.Increment(ref _nextId),
                _bufferSize
            );
        }

        // reset
        args.ResetBuffer();
        args.RemoteEndPoint = null;
        args.SocketError = SocketError.Success;
        
        // forcefully set the internal _socketAddress to null in order to "patch" a SendToAsync bug
        _socketAddressField.SetValue(args, null);

        return args;
    }

    public void Release(ExtendedSocketAsyncEventArgs args)
    {
#if DISABLE_POOL
        args.Dispose();
        return;
#endif
        
        _lock.EnterWriteLock();
        try
        {
            if (_count < _pool.Length)
            {
                _pool[_count++] = args;
                return;
            }

            if (_count == _maxPoolSize)
            {
                args.Dispose();
                return;
            }

            // resize pool
            var newSize = _pool.Length * 2;
            if (newSize > _maxPoolSize)
            {
                newSize = _maxPoolSize;
            }

            Array.Resize(ref _pool, newSize);
            _pool[_count++] = args;
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }
}