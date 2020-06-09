/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonUtils.h

Abstract:
- Helpers for the TerminalApp project
Author(s):
- Mike Griese - August 2019

--*/
#pragma once

namespace TerminalApp::JsonUtils
{
    void GetOptionalColor(const Json::Value& json,
                          std::string_view key,
                          std::optional<til::color>& target);

    void GetOptionalString(const Json::Value& json,
                           std::string_view key,
                           std::optional<std::wstring>& target);

    void GetOptionalGuid(const Json::Value& json,
                         std::string_view key,
                         std::optional<GUID>& target);

    void GetOptionalDouble(const Json::Value& json,
                           std::string_view key,
                           std::optional<double>& target);

    // Method Description:
    // - Helper that can be used for retrieving an optional value from a json
    //   object, and parsing it's value to layer on a given target object.
    //    - If the key we're looking for _doesn't_ exist in the json object,
    //      we'll leave the target object unmodified.
    //    - If the key exists in the json object, but is set to `null`, then
    //      we'll instead set the target back to nullopt.
    // - Each caller should provide a conversion function that takes a
    //   Json::Value and returns an object of the same type as target.
    // Arguments:
    // - json: The json object to search for the given key
    // - key: The key to look for in the json object
    // - target: the optional object to receive the value from json
    // - conversion: a std::function<T(const Json::Value&)> which can be used to
    //   convert the Json::Value to the appropriate type.
    // - validation: optional, if provided, will be called first to ensure that
    //   the json::value is of the correct type before attempting to call
    //   `conversion`.
    // Return Value:
    // - <none>
    template<typename T, typename F>
    void GetOptionalValue(const Json::Value& json,
                          std::string_view key,
                          std::optional<T>& target,
                          F&& conversion,
                          const std::function<bool(const Json::Value&)>& validation = nullptr)
    {
        if (json.isMember(JsonKey(key)))
        {
            if (auto jsonVal{ json[JsonKey(key)] })
            {
                if (validation == nullptr || validation(jsonVal))
                {
                    target = conversion(jsonVal);
                }
            }
            else
            {
                // This branch is hit when the json object contained the key,
                // but the key was set to `null`. In this case, explicitly clear
                // the target.
                target = std::nullopt;
            }
        }
    }

    // Method Description:
    // - Helper that can be used for retrieving a value from a json
    //   object, and parsing it's value to set on a given target object.
    //    - If the key we're looking for _doesn't_ exist in the json object,
    //      we'll leave the target object unmodified.
    //    - If the key exists in the json object, we'll use the provided
    //      `validation` function to ensure that the json value is of the
    //      correct type.
    //    - If we successfully validate the json value type (or no validation
    //      function was provided), then we'll use `conversion` to parse the
    //      value and place the result into `target`
    // - Each caller should provide a conversion function that takes a
    //   Json::Value and returns an object of the same type as target.
    // - Unlike GetOptionalValue, if the key exists but is set to `null`, we'll
    //   just ignore it.
    // Arguments:
    // - json: The json object to search for the given key
    // - key: The key to look for in the json object
    // - target: the optional object to receive the value from json
    // - conversion: a std::function<T(const Json::Value&)> which can be used to
    //   convert the Json::Value to the appropriate type.
    // - validation: optional, if provided, will be called first to ensure that
    //   the json::value is of the correct type before attempting to call
    //   `conversion`.
    // Return Value:
    // - <none>
    template<typename T, typename F>
    void GetValue(const Json::Value& json,
                  std::string_view key,
                  T& target,
                  F&& conversion,
                  const std::function<bool(const Json::Value&)>& validation = nullptr)
    {
        if (json.isMember(JsonKey(key)))
        {
            if (auto jsonVal{ json[JsonKey(key)] })
            {
                if (validation == nullptr || validation(jsonVal))
                {
                    target = conversion(jsonVal);
                }
            }
        }
    }

    void GetInt(const Json::Value& json,
                std::string_view key,
                int& target);

    void GetUInt(const Json::Value& json,
                 std::string_view key,
                 uint32_t& target);

    void GetDouble(const Json::Value& json,
                   std::string_view key,
                   double& target);

    void GetBool(const Json::Value& json,
                 std::string_view key,
                 bool& target);

    void GetWstring(const Json::Value& json,
                    std::string_view key,
                    std::wstring& target);
};
