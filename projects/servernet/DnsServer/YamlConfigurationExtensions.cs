using System.Text;
using YamlDotNet.RepresentationModel;

namespace DnsServer;

public static class YamlConfigurationExtensions
{
    private static string LoadFile(string filename)
    {
        using var stream = File.OpenRead(filename);
        using var sr = new StreamReader(stream, Encoding.UTF8, true, 8192, leaveOpen: true);
        return sr.ReadToEnd();
    }
    
    public static IConfigurationBuilder AddYamlFile(
        this IConfigurationBuilder builder,
        string filename,
        bool optional = true)
    {
        if (!File.Exists(filename))
        {
            if (optional)
            {
                return builder;
            }

            throw new FileNotFoundException("Yaml appsettings not found", filename);
        }

        var data = LoadFile(filename);

        using var input = new StringReader(data);
        
        var yaml = new YamlStream();
        yaml.Load(input);
        
        if (!yaml.Documents.Any())
        {
            // no documents found
            return builder;
        }

        var mapping = (YamlMappingNode)yaml.Documents[0].RootNode;
        foreach (var entry in mapping.Children)
        {
            Console.WriteLine(((YamlScalarNode)entry.Key).Value);
        }

        // List all the items
        var items = (YamlSequenceNode)mapping.Children[new YamlScalarNode("items")];
        foreach (YamlMappingNode item in items)
        {
            Console.WriteLine(
                "{0}\t{1}",
                item.Children[new YamlScalarNode("part_no")],
                item.Children[new YamlScalarNode("descrip")]
            );
        }
        
        return builder;
    }
}