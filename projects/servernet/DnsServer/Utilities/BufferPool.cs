namespace DnsServer.Utilities;

public class PooledBuffer
{
    public byte[] Data { get; }
    
    public int Offset { get; set; }
    
    public int Length { get; set; }

    public PooledBuffer(byte[] data)
    {
        Data = data;
    }

    public void Reset()
    {
        Offset = 0;
        Length = 0;
    }
}

public interface IBufferPool
{
    PooledBuffer Acquire();
    
    void Release(PooledBuffer buffer);
}

public class BufferPool : IDisposable, IBufferPool
{
    private readonly int _bufferSize;
    private readonly Stack<PooledBuffer> _buffers = new();
    private readonly ReaderWriterLockSlim _buffersLock = new(LockRecursionPolicy.NoRecursion);
    
    public BufferPool(int initialBuffersCount, int bufferSize)
    {
        _bufferSize = bufferSize;
        
        for (var i = 0; i < initialBuffersCount; i++)
        {
            _buffers.Push(new PooledBuffer(new byte[bufferSize]));
        }
    }
    
    public void Dispose()
    {
        _buffersLock.Dispose();
    }

    public PooledBuffer Acquire()
    {
        _buffersLock.EnterWriteLock();
        try
        {
            if (!_buffers.TryPop(out var buffer))
            {
                buffer = new PooledBuffer(new byte[_bufferSize]);
            }

            return buffer;
        }
        finally
        {
            _buffersLock.ExitWriteLock();
        }
    }

    public void Release(PooledBuffer buffer)
    {
        _buffersLock.EnterWriteLock();
        try
        {
            buffer.Reset();
            _buffers.Push(buffer);
        }
        finally
        {
            _buffersLock.ExitWriteLock();
        }
    }
}