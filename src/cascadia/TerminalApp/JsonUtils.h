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
                          std::optional<uint32_t>& color);

    void GetOptionalString(const Json::Value& json,
                           std::string_view key,
                           std::optional<std::wstring>& target);

    void GetOptionalGuid(const Json::Value& json,
                         std::string_view key,
                         std::optional<GUID>& target);

    void GetOptionalDouble(const Json::Value& json,
                           std::string_view key,
                           std::optional<double>& target);

    // Be careful, if you pass a lambda stright into this, it will explode in
    // the linker.
    template<typename T, typename F>
    void GetOptionalValue(const Json::Value& json,
                          std::string_view key,
                          std::optional<T>& target,
                          F&& conversion)
    {
        if (json.isMember(JsonKey(key)))
        {
            if (auto jsonVal{ json[JsonKey(key)] })
            {
                target = conversion(jsonVal);
            }
            else
            {
                target = std::nullopt;
            }
        }
    }
};
