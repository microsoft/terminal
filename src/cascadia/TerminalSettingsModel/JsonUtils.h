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

        template<typename T>
        struct DeduceOptional
        {
            using Type = typename std::decay<T>::type;
            static constexpr bool IsOptional = false;
        };

        template<typename TOpt>
        struct DeduceOptional<std::optional<TOpt>>
        {
            using Type = typename std::decay<TOpt>::type;
            static constexpr bool IsOptional = true;
        };

        template<typename TOpt>
        struct DeduceOptional<::winrt::Windows::Foundation::IReference<TOpt>>
        {
            using Type = typename std::decay<TOpt>::type;
            static constexpr bool IsOptional = true;
        };

        template<>
        struct DeduceOptional<::winrt::hstring>
        {
            using Type = typename ::winrt::hstring;
            static constexpr bool IsOptional = true;
        };
    }

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
            return winrt::hstring{ til::u8u16(Detail::GetStringView(json)) };
        }

        Json::Value ToJson(const winrt::hstring& val)
        {
            return til::u16u8(val);
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
            e.expectedType = TypeDescription();
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
            FAIL_FAST();
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
                        e.expectedType = BaseEnumMapper::TypeDescription();
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
                        (val & pair.second) == pair.second)
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
        if constexpr (Detail::DeduceOptional<T>::IsOptional)
        {
            // FOR OPTION TYPES
            // - If the json object is set to `null`, then
            //   we'll instead set the target back to the empty optional.
            if (json.isNull())
            {
                target = T{}; // zero-construct an empty optional
                return true;
            }
        }

        if (json)
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
        return false;
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
        return GetValue(json, target, ConversionTrait<typename Detail::DeduceOptional<T>::Type>{});
    }

    // GetValue, forced return type, with automatic converter
    template<typename T>
    std::decay_t<T> GetValue(const Json::Value& json)
    {
        std::decay_t<T> local{};
        GetValue(json, local, ConversionTrait<typename Detail::DeduceOptional<T>::Type>{});
        return local; // returns zero-initialized or value
    }

    // GetValueForKey, type-deduced, with automatic converter
    template<typename T>
    bool GetValueForKey(const Json::Value& json, std::string_view key, T& target)
    {
        return GetValueForKey(json, key, target, ConversionTrait<typename Detail::DeduceOptional<T>::Type>{});
    }

    // GetValueForKey, forced return type, with automatic converter
    template<typename T>
    std::decay_t<T> GetValueForKey(const Json::Value& json, std::string_view key)
    {
        return GetValueForKey<T>(json, key, ConversionTrait<typename Detail::DeduceOptional<T>::Type>{});
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
        // demand guarantees that it will return a value or throw an exception
        *json.demand(&*key.cbegin(), (&*key.cbegin()) + key.size()) = conv.ToJson(target);
    }

    // SetValueForKey, type-deduced, with automatic converter
    template<typename T>
    void SetValueForKey(Json::Value& json, std::string_view key, const T& target)
    {
        SetValueForKey(json, key, target, ConversionTrait<typename Detail::DeduceOptional<T>::Type>{});
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
