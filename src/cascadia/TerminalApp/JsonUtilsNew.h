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
    // If we don't use winrt, nobody will include the ConversionTrait for winrt::guid.
    // If nobody includes it, this forward declaration will suffice.
    struct guid;
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
            using Type = T;
        };

        template<typename TOpt>
        struct DeduceOptional<std::optional<TOpt>>
        {
            using Type = TOpt;
        };
    }

    class TypeMismatchException : public std::runtime_error
    {
    public:
        TypeMismatchException() :
            runtime_error("invalid type") {}
    };

    class KeyedException : public std::runtime_error
    {
    public:
        KeyedException(const std::string_view key, const std::exception& exception) :
            runtime_error(fmt::format("error parsing \"{0}\": {1}", key, exception.what()).c_str()) {}
    };

    template<typename T>
    struct ConversionTrait
    {
        // FromJson, CanConvert are not defined so as to cause a compile error (which forces a specialization)
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
            return ::Microsoft::Console::Utils::GuidFromString(Detail::GetStringView(json));
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
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
            return json.isString();
        }
    };

    template<typename T, typename TBase>
    struct EnumMapper
    {
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
            // the first mapping is the "Default"
            return TBase::mappings[0].second;
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString();
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

    // Method Description:
    // - Overload on GetValue that will populate a std::optional with a value converted from json
    //    - If the json value doesn't exist we'll leave the target object unmodified.
    //    - If the json object is set to `null`, then
    //      we'll instead set the target back to nullopt.
    // Arguments:
    // - json: the json object to convert
    // - target: the value to populate with the converted result
    // Return Value:
    // - a boolean indicating whether the optional was changed
    //
    // GetValue, type-deduced for optional, manual converter
    template<typename TOpt, typename Converter>
    bool GetValue(const Json::Value& json, std::optional<TOpt>& target, Converter&& conv)
    {
        if (json.isNull())
        {
            target = std::nullopt;
            return true; // null is valid for optionals
        }

        TOpt local{};
        if (GetValue(json, local, std::forward<Converter>(conv)))
        {
            target = std::move(local);
            return true;
        }
        return false;
    }

    // GetValue, forced return type, manual converter
    template<typename T, typename Converter>
    T GetValue(const Json::Value& json, Converter&& conv)
    {
        T local{};
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
            catch (const std::exception& e)
            {
                // WRAP! WRAP LIKE YOUR LIFE DEPENDS ON IT!
                throw KeyedException(key, e);
            }
        }
        return false;
    }

    // GetValueForKey, forced return type, manual converter
    template<typename T, typename Converter>
    T GetValueForKey(const Json::Value& json, std::string_view key, Converter&& conv)
    {
        T local{};
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
    T GetValue(const Json::Value& json)
    {
        T local{};
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
    T GetValueForKey(const Json::Value& json, std::string_view key)
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
