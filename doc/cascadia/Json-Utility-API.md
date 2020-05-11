# New Json Utility API

## Raw value conversion (GetValue)

`GetValue` is a convenience helper that will either read a value into existing storage (type-deduced) or
return a JSON value coerced into the specified type.

When reading into existing storage, it returns a boolean indicating whether that storage was modified.

If the JSON value cannot be converted to the specified type, an exception will be generated.

```c++
std::string one;
std::optional<std::string> two;

JsonUtils::GetValue(json, one);
// one is populated or unchanged.

JsonUtils::GetValue(json, two);
// two is populated, nullopt or unchanged

auto three = JsonUtils::GetValue<std::string>(json);
// three is populated or zero-initialized

auto four = JsonUtils::GetValue<std::optional<std::string>>(json);
// four is populated or nullopt
```

## Key lookup (GetValueForKey)

`GetValueForKey` follows the same rules as `GetValue`, but takes an additional key.
It is assumed that the JSON value passed to GetValueForKey is of `object` type.

```c++
std::string one;
std::optional<std::string> two;

JsonUtils::GetValueForKey(json, "firstKey", one);
// one is populated or unchanged.

JsonUtils::GetValueForKey(json, "secondKey", two);
// two is populated, nullopt or unchanged

auto three = JsonUtils::GetValueForKey<std::string>(json, "thirdKey");
// three is populated or zero-initialized

auto four = JsonUtils::GetValueForKey<std::optional<std::string>>(json, "fourthKey");
// four is populated or nullopt
```

## Converting User-Defined Types

All conversions are done using specializations of `JsonUtils::ConversionTrait<T>`.
To implement a converter for a user-defined type, you must implement a specialization of `JsonUtils::ConversionTrait<T>`.

Every specialization over `T` must implement `static T FromJson(const Json::Value&)` and
`static bool CanConvert(const Json::Value&)`.

```c++
template<>
struct ConversionTrait<MyCustomType>
{
    // This trait converts a string of the format "[0-9]" to a value of type MyCustomType.

    static MyCustomType FromJson(const Json::Value& json)
    {
        return MyCustomType{ json.asString()[0] - '0' };
    }

    static bool CanConvert(const Json::Value& json)
    {
        return json.isString();
    }
};
```

For your "convenience" (;P), if you need to provide name-value enum mapping,
there's the `EnumMapper<>` base template. It is somewhat verbose.

```c++
template<>
struct JsonUtils::ConversionTrait<CursorStyle> : public JsonUtils::EnumMapper<CursorStyle, JsonUtils::ConversionTrait<CursorStyle>>
{
    // Unfortunately, you need to repeat the enum type three whole times ^

    // pair_type is provided by EnumMapper to make your life easier.
    // Unfortunately, you need to provide  v- there  a count of the values in the enum.
    static constexpr std::array<pair_type, 5> mappings = {
        pair_type{ "bar", CursorStyle::Bar }, // DEFAULT
        pair_type{ "vintage", CursorStyle::Vintage },
        pair_type{ "underscore", CursorStyle::Underscore },
        pair_type{ "filledBox", CursorStyle::FilledBox },
        pair_type{ "emptyBox", CursorStyle::EmptyBox }
    };
};
```

### Advanced Use

`GetValue` and `GetValueForKey` can be passed, as their final arguments, any value whose type implements the same
interface as `ConversionTrait<T>`--that is, `FromJson(const Json::Value&)` and `CanConvert(const Json::Value&)`.

This allows for one-off conversions without a specialization of `ConversionTrait` or even stateful converters.

#### Stateful Converter Sample

```c++
struct MultiplyingConverter {
    int BaseValue;

    bool CanConvert(const Json::Value&) { return true; }

    int FromJson(const Json::Value& value)
    {
        return value.asInt() * BaseValue;
    }
};

...

Json::Value jv = /* a json value containing 66 */;
MultiplyingConverter conv{10};

auto v = JsonUtils::GetValue<int>(jv);
// v is equal to 660.
```

## Behavior Chart

### GetValue(T&) (type-deducing)

-|json type invalid|json null|valid
-|-|-|-
`T`|❌ exception|🔵 unchanged|✔ converted
`std::optional<T>`|❌ exception|🟨 `nullopt`|✔ converted

### GetValue&lt;T&gt;() (returning)

-|json type invalid|json null|valid
-|-|-|-
`T`|❌ exception|🟨 `T{}` (zero value)|✔ converted
`std::optional<T>`|❌ exception|🟨 `nullopt`|✔ converted

### GetValueForKey(T&) (type-deducing)

GetValueForKey builds on the behavior set from GetValue by adding
a "key not found" state. The remaining three cases are the same.

val type|key not found|_json type invalid_|_json null_|_valid_
-|-|-|-|-
`T`|🔵 unchanged|_❌ exception_|_🔵 unchanged_|_✔ converted_
`std::optional<T>`|_🔵 unchanged_|_❌ exception_|_🟨 `nullopt`_|_✔ converted_

### GetValueForKey&lt;T&gt;() (return value)

val type|key not found|_json type invalid_|_json null_|_valid_
-|-|-|-|-
`T`|🟨 `T{}` (zero value)|_❌ exception_|_🟨 `T{}` (zero value)_|_✔ converted_
`std::optional<T>`|🟨 `nullopt`|_❌ exception_|_🟨 `nullopt`_|_✔ converted_

### Future Direction

Perhaps it would be reasonable to supply a default CanConvert that returns true to reduce boilerplate.
