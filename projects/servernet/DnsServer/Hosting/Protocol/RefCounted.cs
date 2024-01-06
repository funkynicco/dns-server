namespace DnsServer.Hosting.Protocol;

public class RefCounted<T> : IDisposable where T : IDisposable
{
    class State
    {
        public int References;
    }

    private readonly State _state;

    public T Value { get; }

    private RefCounted(T value, State state)
    {
        _state = state;
        Value = value;
    }

    public RefCounted<T> Copy()
    {
        Interlocked.Increment(ref _state.References);
        return new(Value, _state);
    }

    public void Dispose()
    {
        if (Interlocked.Decrement(ref _state.References) == 0)
        {
            Value.Dispose();
        }
    }

    public static RefCounted<T> Create(T initialValue)
    {
        var state = new State
        {
            References = 1
        };

        return new(initialValue, state);
    }
}