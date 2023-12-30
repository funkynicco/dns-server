using System.Text.Json;

namespace DnsServer.Serializer;

public class JsonSnakeCaseSerializer : IJsonSerializer
{
    private readonly JsonSerializerOptions _options;
    
    public JsonSnakeCaseSerializer(JsonSerializerOptions? options = null)
    {
        _options = options ?? new JsonSerializerOptions();
        _options.PropertyNamingPolicy = new JsonSnakeCaseNamingPolicy();
    }

    public JsonSerializerOptions GetOptions()
        => _options;

    public string Serialize<T>(T value)
        => JsonSerializer.Serialize(value, _options);

    public string SerializeObject(object value)
        => JsonSerializer.Serialize(value, value.GetType(), _options);

    public T? Deserialize<T>(string json)
        => JsonSerializer.Deserialize<T>(json, _options);

    public dynamic DeserializeToAnonymous(string json)
        => JsonSerializer.Deserialize<dynamic>(json, _options)!;

    public string ConvertName(string name)
        => _options.PropertyNamingPolicy?.ConvertName(name) ?? name;
}