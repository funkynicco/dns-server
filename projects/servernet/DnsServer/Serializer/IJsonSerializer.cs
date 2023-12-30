using System.Text.Json;

namespace DnsServer.Serializer;

public interface IJsonSerializer
{
    JsonSerializerOptions GetOptions();
    
    string Serialize<T>(T value);

    string SerializeObject(object value);

    T? Deserialize<T>(string json);

    dynamic DeserializeToAnonymous(string json);

    string ConvertName(string name);
}