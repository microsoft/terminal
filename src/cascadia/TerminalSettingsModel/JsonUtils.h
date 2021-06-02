/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonUtils.h

Abstract:
- Helpers for the Terminal Settings Model project
Author(s):
- Mike Griese - August 2019
- Dustin Howett - January 2020
--*/

#pragma once

#include <json.h>

#include "../types/inc/utils.hpp"

namespace winrt
{
    // If we don't use winrt, nobody will include the ConversionTraits for winrt stuff.
    // If nobody includes it, these forward declarations will suffice.
    struct guid;
    struct hstring;
    namespace Windows::Foundation
    {
        template<typename T>
        struct IReference;
    }
}

// Method Description:
// - Create a std::string from a string_view. We do this because we can't look
//   up a key in a Json::Value with a string_view directly, so instead we'll use
//   this helper. Should a string_view lookup ever be added to jsoncpp, we can
//   remove this entirely.
// Arguments:
// - key: the string_view to build a string from
// Return Value:
// - a std::string to use for looking up a value from a Json::Value
inline std::string JsonKey(const std::string_view key)
{
    return static_cast<std::string>(key);
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    namespace Detail
    {
        // Function Description:
        // - Returns a string_view to a Json::Value's internal string storage,
        //   hopefully without copying it.
        __declspec(noinline) inline const std::string_view GetStringView(const Json::Value& json)
        {
            const char* begin{ nullptr };
            const char* end{ nullptr };
            json.getString(&begin, &end);
            const std::string_view zeroCopyString{ begin, gsl::narrow_cast<size_t>(end - begin) };
            return zeroCopyString;
        }
    }

    template<typename T>
    struct OptionOracle
    {
        template<typename U> // universal parameter
        static constexpr bool HasValue(U&&)
        {
            return true;
        }
    };

    template<typename T>
    struct OptionOracle<std::optional<T>>
    {
        static constexpr std::optional<T> EmptyV() { return std::nullopt; }
        static constexpr bool HasValue(const std::optional<T>& o) { return o.has_value(); }
        // We can return a reference here because the original value is stored inside an std::optional
        static constexpr auto&& Value(const std::optional<T>& o) { return *o; }
    };

    template<typename T>
    struct OptionOracle<::winrt::Windows::Foundation::IReference<T>>
    {
        static constexpr ::winrt::Windows::Foundation::IReference<T> EmptyV() { return nullptr; }
        static constexpr bool HasValue(const ::winrt::Windows::Foundation::IReference<T>& o) { return static_cast<bool>(o); }
        // We CANNOT return a reference here because IReference does NOT return a reference to its internal memory
        static constexpr auto Value(const ::winrt::Windows::Foundation::IReference<T>& o) { return o.Value(); }
    };

    class SerializationError : public std::runtime_error
    {
    public:
        SerializationError() :
            runtime_error("failed to serialize") {}
    };

    class DeserializationError : public std::runtime_error
    {
    public:
        DeserializationError(const Json::Value& value) :
            runtime_error("failed to deserialize"),
            jsonValue{ value } {}

        void SetKey(std::string_view newKey)
        {
            if (!key)
            {
                key = newKey;
            }
        }

        std::optional<std::string> key;
        Json::Value jsonValue;
        std::string expectedType;
    };

    template<typename T>
    struct ConversionTrait
    {
        // Forward-declare these so the linker can pick up specializations from elsewhere!
        T FromJson(const Json::Value&);
        bool CanConvert(const Json::Value& json);

        Json::Value ToJson(const T& val);

        std::string TypeDescription() const { return "<unknown>"; }
    };

    template<>
    struct ConversionTrait<std::string>
    {
        std::string FromJson(const Json::Value& json)
        {
            return json.asString();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }

        Json::Value ToJson(const std::string& val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "string";
        }
    };

    template<>
    struct ConversionTrait<std::wstring>
    {
        std::wstring FromJson(const Json::Value& json)
        {
            return til::u8u16(Detail::GetStringView(json));
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }

        Json::Value ToJson(const std::wstring& val)
        {
            return til::u16u8(val);
        }

        std::string TypeDescription() const
        {
            return "string";
        }
    };

#ifdef WINRT_BASE_H
    template<>
    struct ConversionTrait<winrt::hstring> : public ConversionTrait<std::wstring>
    {
        // Leverage the wstring converter's validation
        winrt::hstring FromJson(const Json::Value& json)
        {
            if (json.isNull())
            {
                return winrt::hstring{};
            }
            return winrt::hstring{ til::u8u16(Detail::GetStringView(json)) };
        }

        Json::Value ToJson(const winrt::hstring& val)
        {
            if (val == winrt::hstring{})
            {
                return Json::Value::nullSingleton();
            }
            return til::u16u8(val);
        }

        bool CanConvert(const Json::Value& json)
        {
            // hstring has a specific behavior for null, so it can convert it
            return ConversionTrait<std::wstring>::CanConvert(json) || json.isNull();
        }
    };
#endif

    template<>
    struct ConversionTrait<bool>
    {
        bool FromJson(const Json::Value& json)
        {
            return json.asBool();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isBool();
        }

        Json::Value ToJson(const bool val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "true | false";
        }
    };

    template<>
    struct ConversionTrait<int>
    {
        int FromJson(const Json::Value& json)
        {
            return json.asInt();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isInt();
        }

        Json::Value ToJson(const int& val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "number";
        }
    };

    template<>
    struct ConversionTrait<unsigned int>
    {
        unsigned int FromJson(const Json::Value& json)
        {
            return json.asUInt();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isUInt();
        }

        Json::Value ToJson(const unsigned int& val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "number (>= 0)";
        }
    };

    template<>
    struct ConversionTrait<float>
    {
        float FromJson(const Json::Value& json)
        {
            return json.asFloat();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isNumeric();
        }

        Json::Value ToJson(const float& val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "number";
        }
    };

    template<>
    struct ConversionTrait<double>
    {
        double FromJson(const Json::Value& json)
        {
            return json.asDouble();
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isNumeric();
        }

        Json::Value ToJson(const double& val)
        {
            return val;
        }

        std::string TypeDescription() const
        {
            return "number";
        }
    };

    template<>
    struct ConversionTrait<GUID>
    {
        GUID FromJson(const Json::Value& json)
        {
            return ::Microsoft::Console::Utils::GuidFromString(til::u8u16(Detail::GetStringView(json)));
        }

        bool CanConvert(const Json::Value& json)
        {
            if (!json.isString())
            {
                return false;
            }

            const auto string{ Detail::GetStringView(json) };
            return string.length() == 38 && string.front() == '{' && string.back() == '}';
        }

        Json::Value ToJson(const GUID& val)
        {
            return til::u16u8(::Microsoft::Console::Utils::GuidToString(val));
        }

        std::string TypeDescription() const
        {
            return "guid";
        }
    };

    // GUID and winrt::guid are mutually convertible,
    // but IReference<winrt::guid> throws some of this off
    template<>
    struct ConversionTrait<winrt::guid>
    {
        winrt::guid FromJson(const Json::Value& json) const
        {
            return static_cast<winrt::guid>(ConversionTrait<GUID>{}.FromJson(json));
        }

        bool CanConvert(const Json::Value& json) const
        {
            return ConversionTrait<GUID>{}.CanConvert(json);
        }

        Json::Value ToJson(const winrt::guid& val)
        {
            return ConversionTrait<GUID>{}.ToJson(val);
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<GUID>{}.TypeDescription();
        }
    };

    template<>
    struct ConversionTrait<til::color>
    {
        til::color FromJson(const Json::Value& json)
        {
            return ::Microsoft::Console::Utils::ColorFromHexString(Detail::GetStringView(json));
        }

        bool CanConvert(const Json::Value& json)
        {
            if (!json.isString())
            {
                return false;
            }

            const auto string{ Detail::GetStringView(json) };
            return (string.length() == 7 || string.length() == 4) && string.front() == '#';
        }

        Json::Value ToJson(const til::color& val)
        {
            return til::u16u8(val.ToHexString(true));
        }

        std::string TypeDescription() const
        {
            return "color (#rrggbb, #rgb)";
        }
    };

#ifdef WINRT_Windows_UI_H
    template<>
    struct ConversionTrait<winrt::Windows::UI::Color>
    {
        winrt::Windows::UI::Color FromJson(const Json::Value& json) const
        {
            return static_cast<winrt::Windows::UI::Color>(ConversionTrait<til::color>{}.FromJson(json));
        }

        bool CanConvert(const Json::Value& json) const
        {
            return ConversionTrait<til::color>{}.CanConvert(json);
        }

        Json::Value ToJson(const winrt::Windows::UI::Color& val)
        {
            return ConversionTrait<til::color>{}.ToJson(val);
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<til::color>{}.TypeDescription();
        }
    };
#endif

#ifdef WINRT_Microsoft_Terminal_Core_H
    template<>
    struct ConversionTrait<winrt::Microsoft::Terminal::Core::Color>
    {
        winrt::Microsoft::Terminal::Core::Color FromJson(const Json::Value& json) const
        {
            return static_cast<winrt::Microsoft::Terminal::Core::Color>(ConversionTrait<til::color>{}.FromJson(json));
        }

        bool CanConvert(const Json::Value& json) const
        {
            return ConversionTrait<til::color>{}.CanConvert(json);
        }

        Json::Value ToJson(const winrt::Microsoft::Terminal::Core::Color& val)
        {
            return ConversionTrait<til::color>{}.ToJson(val);
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<til::color>{}.TypeDescription();
        }
    };
#endif

    template<typename T, typename TDelegatedConverter = ConversionTrait<typename std::decay<T>::type>, typename TOpt = std::optional<typename std::decay<T>::type>>
    struct OptionalConverter
    {
        using Oracle = OptionOracle<TOpt>;
        TDelegatedConverter delegatedConverter{};

        TOpt FromJson(const Json::Value& json)
        {
            if (!json && !delegatedConverter.CanConvert(json))
            {
                // If the nested converter can't deal with null, emit an empty optional
                // If it can, it probably has specific null behavior that it wants to use.
                return Oracle::EmptyV();
            }
            TOpt val{ delegatedConverter.FromJson(json) };
            return val;
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isNull() || delegatedConverter.CanConvert(json);
        }

        Json::Value ToJson(const TOpt& val)
        {
            if (!Oracle::HasValue(val))
            {
                return Json::Value::nullSingleton();
            }
            return delegatedConverter.ToJson(Oracle::Value(val));
        }

        std::string TypeDescription() const
        {
            return delegatedConverter.TypeDescription();
        }
    };

    template<typename T>
    struct ConversionTrait<std::optional<T>> : public OptionalConverter<T, ConversionTrait<T>, std::optional<T>>
    {
    };

#ifdef WINRT_Windows_Foundation_H
    template<typename T>
    struct ConversionTrait<::winrt::Windows::Foundation::IReference<T>> : public OptionalConverter<T, ConversionTrait<T>, ::winrt::Windows::Foundation::IReference<T>>
    {
    };
#endif

    template<typename T, typename TBase>
    struct EnumMapper
    {
        using BaseEnumMapper = EnumMapper<T, TBase>;
        using ValueType = T;
        using pair_type = std::pair<std::string_view, T>;
        T FromJson(const Json::Value& json)
        {
            const auto name{ Detail::GetStringView(json) };
            for (const auto& pair : TBase::mappings)
            {
                if (pair.first == name)
                {
                    return pair.second;
                }
            }

            DeserializationError e{ json };
            e.expectedType = static_cast<const TBase&>(*this).TypeDescription();
            throw e;
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }

        Json::Value ToJson(const T& val)
        {
            for (const auto& pair : TBase::mappings)
            {
                if (pair.second == val)
                {
                    return { pair.first.data() };
                }
            }
            throw SerializationError{};
        }

        std::string TypeDescription() const
        {
            std::vector<std::string_view> names;
            std::transform(TBase::mappings.cbegin(), TBase::mappings.cend(), std::back_inserter(names), [](auto&& p) { return p.first; });
            return fmt::format("{}", fmt::join(names, " | "));
        }
    };

    // FlagMapper is EnumMapper, but it works for bitfields.
    // It supports a string (single flag) or an array of strings.
    // Does an O(n*m) search; meant for small search spaces!
    //
    // Cleverly leverage EnumMapper to do the heavy lifting.
    template<typename T, typename TBase>
    struct FlagMapper : public EnumMapper<T, TBase>
    {
    private:
        // Hide BaseEnumMapper so FlagMapper's consumers cannot see
        // it.
        using BaseEnumMapper = EnumMapper<T, TBase>::BaseEnumMapper;

    public:
        using BaseFlagMapper = FlagMapper<T, TBase>;
        static constexpr T AllSet{ static_cast<T>(~0u) };
        static constexpr T AllClear{ static_cast<T>(0u) };

        T FromJson(const Json::Value& json)
        {
            if (json.isString())
            {
                return BaseEnumMapper::FromJson(json);
            }
            else if (json.isArray())
            {
                unsigned int seen{ 0 };
                T value{};
                for (const auto& element : json)
                {
                    const auto newFlag{ BaseEnumMapper::FromJson(element) };
                    if (++seen > 1 &&
                        ((newFlag == AllClear && value != AllClear) ||
                         (value == AllClear && newFlag != AllClear)))
                    {
                        // attempt to combine AllClear (explicitly) with anything else
                        DeserializationError e{ element };
                        e.expectedType = static_cast<const TBase&>(*this).TypeDescription();
                        throw e;
                    }
                    value |= newFlag;
                }
                return value;
            }

            // We'll only get here if CanConvert has failed us.
            return AllClear;
        }

        Json::Value ToJson(const T& val)
        {
            if (val == AllClear)
            {
                return BaseEnumMapper::ToJson(AllClear);
            }
            else if (val == AllSet)
            {
                return BaseEnumMapper::ToJson(AllSet);
            }
            else if (WI_IsSingleFlagSet(val))
            {
                return BaseEnumMapper::ToJson(val);
            }
            else
            {
                Json::Value json{ Json::ValueType::arrayValue };
                for (const auto& pair : TBase::mappings)
                {
                    if (pair.second != AllClear &&
                        (val & pair.second) == pair.second &&
                        WI_IsSingleFlagSet(pair.second))
                    {
                        json.append(BaseEnumMapper::ToJson(pair.second));
                    }
                }
                return json;
            }
        }

        bool CanConvert(const Json::Value& json)
        {
            return BaseEnumMapper::CanConvert(json) || json.isArray();
        }
    };

    template<typename T>
    struct PermissiveStringConverter
    {
    };

    template<>
    struct PermissiveStringConverter<std::wstring>
    {
        std::wstring FromJson(const Json::Value& json)
        {
            return til::u8u16(json.asString());
        }

        bool CanConvert(const Json::Value& /*unused*/)
        {
            return true;
        }

        std::string TypeDescription() const
        {
            return "any";
        }
    };

    // Method Description:
    // - Helper that will populate a reference with a value converted from a json object.
    // Arguments:
    // - json: the json object to convert
    // - target: the value to populate with the converted result
    // Return Value:
    // - a boolean indicating whether the value existed (in this case, was non-null)
    //
    // GetValue, type-deduced, manual converter
    template<typename T, typename Converter>
    bool GetValue(const Json::Value& json, T& target, Converter&& conv)
    {
        if (!conv.CanConvert(json))
        {
            DeserializationError e{ json };
            e.expectedType = conv.TypeDescription();
            throw e;
        }

        target = conv.FromJson(json);
        return true;
    }

    // GetValue, forced return type, manual converter
    template<typename T, typename Converter>
    std::decay_t<T> GetValue(const Json::Value& json, Converter&& conv)
    {
        std::decay_t<T> local{};
        GetValue(json, local, std::forward<Converter>(conv));
        return local; // returns zero-initialized or value
    }

    // GetValueForKey, type-deduced, manual converter
    template<typename T, typename Converter>
    bool GetValueForKey(const Json::Value& json, std::string_view key, T& target, Converter&& conv)
    {
        if (auto found{ json.find(&*key.cbegin(), (&*key.cbegin()) + key.size()) })
        {
            try
            {
                return GetValue(*found, target, std::forward<Converter>(conv));
            }
            catch (DeserializationError& e)
            {
                e.SetKey(key);
                throw; // rethrow now that it has a key
            }
        }
        return false;
    }

    // GetValueForKey, forced return type, manual converter
    template<typename T, typename Converter>
    std::decay_t<T> GetValueForKey(const Json::Value& json, std::string_view key, Converter&& conv)
    {
        std::decay_t<T> local{};
        GetValueForKey(json, key, local, std::forward<Converter>(conv));
        return local; // returns zero-initialized?
    }

    // GetValue, type-deduced, with automatic converter
    template<typename T>
    bool GetValue(const Json::Value& json, T& target)
    {
        return GetValue(json, target, ConversionTrait<typename std::decay<T>::type>{});
    }

    // GetValue, forced return type, with automatic converter
    template<typename T>
    std::decay_t<T> GetValue(const Json::Value& json)
    {
        std::decay_t<T> local{};
        GetValue(json, local, ConversionTrait<typename std::decay<T>::type>{});
        return local; // returns zero-initialized or value
    }

    // GetValueForKey, type-deduced, with automatic converter
    template<typename T>
    bool GetValueForKey(const Json::Value& json, std::string_view key, T& target)
    {
        return GetValueForKey(json, key, target, ConversionTrait<typename std::decay<T>::type>{});
    }

    // GetValueForKey, forced return type, with automatic converter
    template<typename T>
    std::decay_t<T> GetValueForKey(const Json::Value& json, std::string_view key)
    {
        return GetValueForKey<T>(json, key, ConversionTrait<typename std::decay<T>::type>{});
    }

    // Get multiple values for keys (json, k, &v, k, &v, k, &v, ...).
    // Uses the default converter for each v.
    // Careful: this can cause a template explosion.
    constexpr void GetValuesForKeys(const Json::Value& /*json*/) {}

    template<typename T, typename... Args>
    void GetValuesForKeys(const Json::Value& json, std::string_view key1, T&& val1, Args&&... args)
    {
        GetValueForKey(json, key1, val1);
        GetValuesForKeys(json, std::forward<Args>(args)...);
    }

    // SetValueForKey, type-deduced, manual converter
    template<typename T, typename Converter>
    void SetValueForKey(Json::Value& json, std::string_view key, const T& target, Converter&& conv)
    {
        // We don't want to write any empty optionals into JSON (right now).
        if (OptionOracle<T>::HasValue(target))
        {
            // demand guarantees that it will return a value or throw an exception
            *json.demand(&*key.cbegin(), (&*key.cbegin()) + key.size()) = conv.ToJson(target);
        }
    }

    // SetValueForKey, type-deduced, with automatic converter
    template<typename T>
    void SetValueForKey(Json::Value& json, std::string_view key, const T& target)
    {
        SetValueForKey(json, key, target, ConversionTrait<typename std::decay<T>::type>{});
    }
};

#define JSON_ENUM_MAPPER(...)                                                                \
    template<>                                                                               \
    struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<__VA_ARGS__> : \
        public ::Microsoft::Terminal::Settings::Model::JsonUtils::EnumMapper<__VA_ARGS__, ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<__VA_ARGS__>>

#define JSON_FLAG_MAPPER(...)                                                                \
    template<>                                                                               \
    struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<__VA_ARGS__> : \
        public ::Microsoft::Terminal::Settings::Model::JsonUtils::FlagMapper<__VA_ARGS__, ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<__VA_ARGS__>>

#define JSON_MAPPINGS(Count) \
    static constexpr std::array<pair_type, Count> mappings
