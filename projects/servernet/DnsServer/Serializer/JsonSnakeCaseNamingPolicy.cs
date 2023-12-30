using System.Text;
using System.Text.Json;

namespace DnsServer.Serializer;

public class JsonSnakeCaseNamingPolicy : JsonNamingPolicy
{
    public static JsonNamingPolicy SnakeCase { get; } = new JsonSnakeCaseNamingPolicy();

    public override string ConvertName(string name)
    {
        // HelloWorld => hello_world

        var result = new StringBuilder(name.Length * 2);
        for (var i = 0; i < name.Length; i++)
        {
            if (i > 0 &&
                char.IsUpper(name[i]))
            {
                result.Append('_');
            }

            result.Append(char.ToLowerInvariant(name[i]));
        }

        return result.ToString();
    }
}