using System.Collections;
using System.Reflection;
using System.Text.Json;

namespace DnsServer.Serializer;

public interface IJsonPatchHelper
{
    /// <summary>
    /// Applies the patches represented in json element to the object and returns the amount of changes applied.
    /// </summary>
    int Apply<T>(T obj, JsonElement element) where T : class;
}

public class JsonPatchHelper : IJsonPatchHelper
{
    private readonly IJsonSerializer _jsonSerializer;

    private readonly NullabilityInfoContext _nullabilityInfoContext = new();

    public JsonPatchHelper(IJsonSerializer jsonSerializer)
    {
        _jsonSerializer = jsonSerializer;
    }

    public int Apply<T>(T obj, JsonElement element) where T : class
    {
        if (obj is null)
        {
            throw new ArgumentNullException(nameof(obj));
        }

        if (element.ValueKind != JsonValueKind.Object)
        {
            throw new ArgumentException("The provided JSON to apply as a patch must be a JSON object.", nameof(element));
        }

        var changes = 0;
        InternalApply(ref changes, obj.GetType(), obj, element, false);
        return changes;
    }

    private object? ConstructValue(Type element_type, JsonElement value)
    {
        object? current_value = null;
        try
        {
            current_value = Activator.CreateInstance(element_type);
        }
        catch
        {
            // ignored - this is to satisfy primitive constructors of objects
        }

        var ignore = 0;
        return InternalApply(
            ref ignore,
            element_type,
            current_value,
            value,
            false
        );
    }

    private bool IsNullable(PropertyInfo pi)
        => _nullabilityInfoContext.Create(pi).WriteState != NullabilityState.NotNull;

    private bool IsNullable(FieldInfo fi)
        => _nullabilityInfoContext.Create(fi).WriteState != NullabilityState.NotNull;

    private object? InternalApply(ref int changes, Type obj_type, object? obj_value, JsonElement element, bool nullable)
    {
        if (element.ValueKind != JsonValueKind.Null &&
            ((obj_type.IsClass && obj_type != typeof(string)) ||
             (obj_type.IsValueType && !obj_type.IsEnum && !obj_type.IsPrimitive))) // class or struct only
        {
            if (obj_type.IsArray)
            {
                if (element.ValueKind != JsonValueKind.Array)
                {
                    throw new JsonException("The JSON value provided is not expected array.");
                }

                var element_type = obj_type.GetElementType()!;
                var length = element.GetArrayLength();
                var new_array = Array.CreateInstance(element_type, length);
                var i = 0;
                foreach (var array_value in element.EnumerateArray())
                {
                    new_array.SetValue(
                        ConstructValue(element_type, array_value),
                        i++
                    );
                }

                ++changes;
                return new_array;
            }

            if (obj_type.IsGenericType)
            {
                if (obj_type.GetGenericTypeDefinition() == typeof(List<>))
                {
                    if (element.ValueKind != JsonValueKind.Array)
                    {
                        throw new JsonException("The JSON value provided is not expected array.");
                    }

                    var element_type = obj_type.GenericTypeArguments.First();
                    var list = (IList)Activator.CreateInstance(obj_type)!;

                    foreach (var array_value in element.EnumerateArray())
                    {
                        list.Add(ConstructValue(element_type, array_value));
                    }

                    ++changes;
                    return list;
                }

                if (obj_type.GetGenericTypeDefinition() == typeof(HashSet<>))
                {
                    if (element.ValueKind != JsonValueKind.Array)
                    {
                        throw new JsonException("The JSON value provided is not expected array.");
                    }

                    var element_type = obj_type.GenericTypeArguments.First();
                    var set = Activator.CreateInstance(obj_type)!;
                    var add = obj_type.GetMethod("Add")!;

                    foreach (var array_value in element.EnumerateArray())
                    {
                        add.Invoke(
                            set,
                            new[] { ConstructValue(element_type, array_value) }
                        );
                    }

                    ++changes;
                    return set;
                }

                if (obj_type.GetGenericTypeDefinition() == typeof(Dictionary<,>))
                {
                    if (element.ValueKind != JsonValueKind.Object)
                    {
                        throw new JsonException("The JSON value provided is not expected object.");
                    }

                    if (obj_type.GenericTypeArguments.First() != typeof(string))
                    {
                        throw new Exception($"The key must be a string of dictionary {obj_type}.");
                    }

                    var element_type = obj_type.GenericTypeArguments.Last();
                    var dictionary = (IDictionary)Activator.CreateInstance(obj_type)!;

                    foreach (var property in element.EnumerateObject())
                    {
                        dictionary.Add(
                            property.Name,
                            ConstructValue(element_type, property.Value)
                        );
                    }

                    ++changes;
                    return dictionary;
                }
            }

            var object_members = new Dictionary<string, MemberInfo>();
            foreach (var member in obj_type.GetMembers(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
            {
                switch (member)
                {
                    case PropertyInfo { SetMethod: { } }: // property that has a setter method
                    case FieldInfo:
                        object_members.Add(
                            _jsonSerializer.ConvertName(member.Name),
                            member
                        );
                        break;
                }
            }
            
            if (obj_value is null &&
                obj_type.IsClass &&
                obj_type != typeof(string))
            {
                // attempt to construct the object
                var constructor = obj_type
                    .GetConstructors(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
                    .FirstOrDefault(a => a.GetParameters().Length == 0);

                if (constructor is null)
                {
                    throw new Exception($"Unable to find a suitable constructor for type {obj_type}.");
                }

                obj_value = constructor.Invoke(Array.Empty<object?>());
            }

            foreach (var property in element.EnumerateObject())
            {
                if (!object_members.TryGetValue(property.Name, out var member))
                {
                    continue;
                }

                switch (member)
                {
                    case PropertyInfo pi:
                        pi.SetValue(
                            obj_value,
                            InternalApply(
                                ref changes,
                                pi.PropertyType,
                                pi.GetValue(obj_value),
                                property.Value,
                                IsNullable(pi)
                            )
                        );
                        break;
                    case FieldInfo fi:
                        fi.SetValue(
                            obj_value,
                            InternalApply(
                                ref changes,
                                fi.FieldType,
                                fi.GetValue(obj_value),
                                property.Value,
                                IsNullable(fi)
                            )
                        );
                        break;
                }
            }
        }
        else
        {
            // value type
            obj_value = GetValueToSet(obj_type, element, nullable);
            changes++;
        }

        return obj_value;
    }

    private object? GetValueToSet(Type valueType, JsonElement value, bool nullable)
    {
        if (value.ValueKind == JsonValueKind.Null)
        {
            if (nullable)
            {
                return null;
            }

            if (valueType.IsClass &&
                valueType != typeof(string))
            {
                return null;
            }

            if (valueType.IsGenericType &&
                valueType.GetGenericTypeDefinition() == typeof(Nullable<>))
            {
                return null;
            }

            throw new JsonException("The type must be either class or nullable in order to be set as null.");
        }

        if (valueType == typeof(string))
        {
            return value.GetString() ?? throw new Exception("Unable to get string from json element.");
        }

        if (valueType == typeof(int))
        {
            return value.GetInt32();
        }

        if (valueType == typeof(long))
        {
            return value.GetInt64();
        }

        if (valueType == typeof(bool))
        {
            return value.GetBoolean();
        }

        if (valueType == typeof(float))
        {
            return value.GetSingle();
        }

        if (valueType == typeof(double))
        {
            return value.GetDouble();
        }

        if (valueType == typeof(decimal))
        {
            return value.GetDecimal();
        }

        throw new Exception($"The json value could not be converted into the type {valueType.FullName}");
    }
}