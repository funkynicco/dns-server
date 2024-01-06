namespace DnsServer.Hosting.Protocol;

public struct DnsHeader
{
    public const int Size =
        sizeof(ushort) + // TransactionID
        sizeof(ushort) + // Flags
        sizeof(ushort) + // NumberOfQuestions
        sizeof(ushort) + // NumberOfAnswers
        sizeof(ushort) + // NumberOfAuthorities
        sizeof(ushort); // NumberOfAdditional

    public ushort TransactionId { get; set; }

    public ushort Flags { get; set; }

    public ushort NumberOfQuestions { get; set; }

    public ushort NumberOfAnswers { get; set; }

    public ushort NumberOfAuthorities { get; set; }

    public ushort NumberOfAdditional { get; set; }

    public void Write(byte[] data, int offset)
    {
        data[offset++] = (byte)(TransactionId >> 8);
        data[offset++] = (byte)(TransactionId & 0xff);
        
        data[offset++] = (byte)(Flags >> 8);
        data[offset++] = (byte)(Flags & 0xff);
        
        data[offset++] = (byte)(NumberOfQuestions >> 8);
        data[offset++] = (byte)(NumberOfQuestions & 0xff);
        
        data[offset++] = (byte)(NumberOfAnswers >> 8);
        data[offset++] = (byte)(NumberOfAnswers & 0xff);
        
        data[offset++] = (byte)(NumberOfAuthorities >> 8);
        data[offset++] = (byte)(NumberOfAuthorities & 0xff);
        
        data[offset++] = (byte)(NumberOfAdditional >> 8);
        data[offset] = (byte)(NumberOfAdditional & 0xff);
    }

    public static DnsHeader Decode(byte[] data, int offset)
    {
        var result = new DnsHeader();

        result.TransactionId = (ushort)(data[offset + 0] << 8 | data[offset + 1]);
        result.Flags = (ushort)(data[offset + 2] << 8 | data[offset + 3]);
        result.NumberOfQuestions = (ushort)(data[offset + 4] << 8 | data[offset + 5]);
        result.NumberOfAnswers = (ushort)(data[offset + 6] << 8 | data[offset + 7]);
        result.NumberOfAuthorities = (ushort)(data[offset + 8] << 8 | data[offset + 9]);
        result.NumberOfAdditional = (ushort)(data[offset + 10] << 8 | data[offset + 11]);

        return result;
    }
}

public class DnsHeaderLengthMismatchException : Exception
{
    public DnsHeaderLengthMismatchException() :
        base("The length of the DNS header is not valid.")
    {
    }
}