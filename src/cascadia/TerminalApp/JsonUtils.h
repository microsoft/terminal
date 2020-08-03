/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonUtils.h

Abstract:
- Helpers for the TerminalApp project
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

namespace TerminalApp::JsonUtils
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
    }

    // These exceptions cannot use localized messages, as we do not have
    // guaranteed access to the resource loader.
    class TypeMismatchException : public std::runtime_error
    {
    public:
        TypeMismatchException() :
            runtime_error("unexpected data type") {}
    };

    class KeyedException : public std::runtime_error
    {
    public:
        KeyedException(const std::string_view key, std::exception_ptr exception) :
            runtime_error(fmt::format("error parsing \"{0}\"", key).c_str()),
            _key{ key },
            _innerException{ std::move(exception) } {}

        std::string GetKey() const
        {
            return _key;
        }

        [[noreturn]] void RethrowInner() const
        {
            std::rethrow_exception(_innerException);
        }

    private:
        std::string _key;
        std::exception_ptr _innerException;
    };

    class UnexpectedValueException : public std::runtime_error
    {
    public:
        UnexpectedValueException(const std::string_view value) :
            runtime_error(fmt::format("unexpected value \"{0}\"", value).c_str()),
            _value{ value } {}

        std::string GetValue() const
        {
            return _value;
        }

    private:
        std::string _value;
    };

    template<typename T>
    struct ConversionTrait
    {
        // Forward-declare these so the linker can pick up specializations from elsewhere!
        T FromJson(const Json::Value&);
        bool CanConvert(const Json::Value& json);
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
    };

    // (GUID and winrt::guid are mutually convertible!)
    template<>
    struct ConversionTrait<winrt::guid> : public ConversionTrait<GUID>
    {
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
    };

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

            throw UnexpectedValueException{ name };
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
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
                        throw UnexpectedValueException{ element.asString() };
                    }
                    value |= newFlag;
                }
                return value;
            }

            // We'll only get here if CanConvert has failed us.
            return AllClear;
        }

        bool CanConvert(const Json::Value& json)
        {
            return BaseEnumMapper::CanConvert(json) || json.isArray();
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
                throw TypeMismatchException{};
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
            catch (...)
            {
                // Wrap any caught exceptions in one that preserves context.
                throw KeyedException(key, std::current_exception());
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
};

#define JSON_ENUM_MAPPER(...)                                       \
    template<>                                                      \
    struct ::TerminalApp::JsonUtils::ConversionTrait<__VA_ARGS__> : \
        public ::TerminalApp::JsonUtils::EnumMapper<__VA_ARGS__, ::TerminalApp::JsonUtils::ConversionTrait<__VA_ARGS__>>

#define JSON_FLAG_MAPPER(...)                                       \
    template<>                                                      \
    struct ::TerminalApp::JsonUtils::ConversionTrait<__VA_ARGS__> : \
        public ::TerminalApp::JsonUtils::FlagMapper<__VA_ARGS__, ::TerminalApp::JsonUtils::ConversionTrait<__VA_ARGS__>>

#define JSON_MAPPINGS(Count) \
    static constexpr std::array<pair_type, Count> mappings
